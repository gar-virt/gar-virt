module;

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module virt:api;

import utility;

export namespace ls_gitea_runner {

struct Arch {
    enum Type { amd64, arm64 };

    static std::expected<Arch::Type, GenericError> from_name(std::string_view name) noexcept;
    static std::string to_name(Arch::Type value);
};

std::expected<Arch::Type, GenericError> Arch::from_name(std::string_view name) noexcept {
    using namespace std::literals;

    constexpr static auto amd64_names = {"amd64"sv, "x64"sv, "x86_64"sv, "x86-64"sv};
    for (const auto& s : amd64_names) {
        if (utility::string_compare_ci(name, s) == 0) {
            return Arch::amd64;
        }
    }

    constexpr static auto arm64_names = {"arm64"sv, "aarch64"sv};
    for (const auto& s : arm64_names) {
        if (utility::string_compare_ci(name, s) == 0) {
            return Arch::amd64;
        }
    }

    return std::unexpected{GenericError{std::format("Unsupported arch: {}", name)}};
}

std::string Arch::to_name(Arch::Type value) {
    switch (value) {
    case Arch::amd64:
        return "amd64;";
    case Arch::arm64:
        return "arm64;";
    }
    std::abort();
}

struct SpawnResult {
    int exit_code{};
    std::string output;
};

class Machine {
public:
    struct Info {
        std::string os;
        Arch::Type arch;
        std::string temp_dir;
    };

    virtual ~Machine() = default;
    virtual const std::string& get_id() const = 0;
    virtual std::expected<void, GenericError> terminate() = 0;
    virtual std::expected<SpawnResult, GenericError>
    shell_exec(const std::vector<std::string>& cmd, const std::optional<std::chrono::seconds>& timeout) const = 0;
    virtual std::expected<void, GenericError> wait_for_guest_agent(std::chrono::seconds timeout,
                                                                   const utility::ShutdownSignal& stop) = 0;
    virtual const Info& info() const = 0;

    std::string make_temp_path(const std::string& sub_path) const;

    template <utility::contiguous_byte_container T>
    std::expected<void, GenericError> write_file(const std::string& remote_path, const T& content) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return write_file_impl(remote_path, {reinterpret_cast<const std::byte*>(content.data()), content.size()});
    }

protected:
    virtual std::expected<void, GenericError> write_file_impl(const std::string& remote_path,
                                                              std::span<const std::byte> content) = 0;
};

struct MachinePoolStats {
    size_t provisioned{};
    size_t acquiring{};
    size_t acquired{};
    size_t active{};
    size_t idle{};
    size_t warming{};

    std::strong_ordering operator<=>(const MachinePoolStats&) const = default;
};

class MachinePool final {
    class Impl final {
        struct MachineCounters {
            size_t acquiring{};
            size_t acquired{};
            size_t active{};
            size_t warming{};
        };

        struct IdleMachine {
            std::shared_ptr<Machine> machine;
            std::chrono::steady_clock::time_point since;

            IdleMachine(std::shared_ptr<Machine> machine)
                    : machine{std::move(machine)}, since{std::chrono::steady_clock::now()} {}
        };

    public:
        Impl(size_t idle_target, size_t max_concurrency,
             std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> machine_spawner,
             utility::ShutdownSignal shutdown_signal)
                : m_shutdown_signal{std::move(shutdown_signal)}, m_idle_target{idle_target},
                  m_max_concurrency{max_concurrency}, m_machine_spawner{std::move(machine_spawner)},
                  m_workers{0, max_concurrency} {}

        ~Impl() { stop(); }

        Impl(const Impl&) = delete;
        Impl(Impl&&) = delete;

        Impl& operator=(const Impl&) = delete;
        Impl& operator=(Impl&&) = delete;

        std::expected<std::shared_ptr<Machine>, GenericError> acquire(std::chrono::milliseconds timeout) {
            using namespace std::chrono_literals;
            std::unique_lock lock{m_mutex};
            ++m_machine_counters.acquiring;
            check_stats(lock);
            const auto start_time{std::chrono::steady_clock::now()};
            while (true) {
                const auto timed_out{
                    !m_idle_cv.wait_for(lock, 500ms, [this] { return should_stop() || !m_idle_machines.empty(); })};
                if (should_stop()) {
                    --m_machine_counters.acquiring;
                    return std::unexpected{GenericError{"Shutting down machine pool"}};
                }
                if (timed_out) {
                    if (std::chrono::steady_clock::now() - start_time < timeout) {
                        continue;
                    }
                    break;
                }
                auto idle_machine{std::move(m_idle_machines.front())};
                m_idle_machines.pop();
                --m_machine_counters.acquiring;
                ++m_machine_counters.acquired;
                check_stats(lock);
                m_idle_cv.notify_one();
                return idle_machine.machine;
            }
            --m_machine_counters.acquiring;
            return std::unexpected{GenericError{"Timed out while acquiring machine"}};
        }

        // NOLINTNEXTLINE(performance-unnecessary-value-param): May want to own machine later (tracking)
        void activate(std::shared_ptr<Machine> /*machine*/) {
            std::unique_lock lock{m_mutex};
            ++m_machine_counters.active;
            check_stats(lock);
            lock.unlock();
            m_control_cv.notify_one();
        }

        // NOLINTNEXTLINE(performance-unnecessary-value-param): May want to own machine later (tracking)
        void deactivate(std::shared_ptr<Machine> /*machine*/) {
            const std::unique_lock lock{m_mutex};
            --m_machine_counters.active;
        }

        void release(std::shared_ptr<Machine> machine) {
            machine.reset();
            std::unique_lock lock{m_mutex};
            --m_machine_counters.acquired;
            check_stats(lock);
            lock.unlock();
            m_control_cv.notify_one();
        }

        void start() {
            const std::unique_lock lock{m_mutex};
            m_control_worker = std::jthread{[this] { control_loop(); }};
        }

        void stop() {
            {
                std::unique_lock lock{m_mutex};
                stop_internal(lock);
            }
            if (m_control_worker.joinable()) {
                m_control_worker.join();
            }
            m_workers.stop();
        }

        void set_stats_callback(std::move_only_function<void(const MachinePoolStats&) noexcept> cb) {
            const std::scoped_lock lock{m_mutex};
            m_stats_cb = std::move(cb);
        }

    private:
        void control_loop() {
            using namespace std::chrono_literals;
            size_t fail_count{};
            while (true) {
                std::unique_lock lock{m_mutex};
                if (should_stop()) {
                    stop_internal(lock);
                    return;
                }
                if (!m_control_cv.wait_for(lock, 500ms, [this] { return should_stop() || want_upscale(); })) {
                    continue;
                }
                if (should_stop()) {
                    continue;
                }
                if (want_upscale()) {
                    if (auto spawn_res{add_spawner(lock)}) {
                        fail_count = 0;
                    } else {
                        ++fail_count;
                        const auto backoff{std::min(60s, 1s * static_cast<std::make_signed_t<decltype(fail_count)>>(
                                                                  utility::ipow2(std::min(fail_count, size_t{6}))))};
                        const auto backoff_s{std::chrono::duration_cast<std::chrono::seconds>(backoff)};
                        global_logger().error("Failed to add spawner {} time(s), backing off {}s: {}", fail_count,
                                              backoff_s.count(), spawn_res.error().what());
                        m_control_cv.wait_for(lock, backoff, [this] { return should_stop(); });
                    }
                }
            }
        }

        void stop_internal(std::unique_lock<std::mutex>& acquired_lock) {
            auto notify{[&] {
                acquired_lock.unlock();
                m_idle_cv.notify_all();
                m_control_cv.notify_all();
                acquired_lock.lock();
            }};
            if (m_stop) {
                notify();
                return;
            }
            m_stop = true;
            notify();
        }

        std::expected<void, GenericError> add_spawner(std::unique_lock<std::mutex>& acquired_lock) {
            ++m_machine_counters.warming;
            check_stats(acquired_lock);
            // TODO: Can we get rid of the try-catch?
            try {
                m_workers.put([this] { spawn_one(); });
                return {};
            } catch (const std::exception& ex) {
                --m_machine_counters.warming;
                check_stats(acquired_lock);
                return std::unexpected{GenericError{std::format("Failed to submit spawn task: {}", ex.what())}};
            }
        }

        // We want the number of currently active machines plus another, but no more than max
        bool want_upscale() {
            const auto provisioned{get_provisioned_count()};
            const auto target{m_idle_target + m_machine_counters.active};
            const auto decision{provisioned < target && provisioned < m_max_concurrency};
            return decision;
        }

        void spawn_one() {
            {
                // FIXME: Need to back off if this keeps failing, otherwise control loop will run hot
                auto machine_res{m_machine_spawner()};
                std::unique_lock lock{m_mutex};
                --m_machine_counters.warming;
                if (machine_res) {
                    m_idle_machines.emplace(*std::move(machine_res));
                } else {
                    global_logger().error("Failed to spawn machine: {}", machine_res.error().what());
                }
                check_stats(lock);
            }
            m_idle_cv.notify_one();
        }

        size_t get_provisioned_count() const noexcept {
            return m_machine_counters.acquired + m_idle_machines.size() + m_machine_counters.warming;
        }

        void check_stats(std::unique_lock<std::mutex>& acquired_lock) {
            const MachinePoolStats temp_stats{
                .provisioned = get_provisioned_count(),
                .acquiring = m_machine_counters.acquiring,
                .acquired = m_machine_counters.acquired,
                .active = m_machine_counters.active,
                .idle = m_idle_machines.size(),
                .warming = m_machine_counters.warming,
            };
            if (m_stats == temp_stats) {
                return;
            }
            m_stats = temp_stats;
            report_stats(acquired_lock);
        }

        void report_stats(std::unique_lock<std::mutex>& acquired_lock) {
            if (!m_stats_cb) {
                return;
            }
            const auto snapshot{m_stats};
            acquired_lock.unlock();
            try {
                m_stats_cb(snapshot);
            } catch (...) { // NOLINT(bugprone-empty-catch)
                // Ignore exceptions in callback
            }
            acquired_lock.lock();
        }

        bool should_stop() const { return m_stop || m_shutdown_signal.is_signalled(); }

        utility::ShutdownSignal m_shutdown_signal;
        size_t m_idle_target{};
        size_t m_max_concurrency{};
        bool m_stop{};
        MachineCounters m_machine_counters;
        std::queue<IdleMachine> m_idle_machines;
        mutable std::mutex m_mutex;
        std::condition_variable m_idle_cv;
        std::condition_variable m_control_cv;
        MachinePoolStats m_stats;
        std::move_only_function<void(const MachinePoolStats&) noexcept> m_stats_cb;
        std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> m_machine_spawner;
        utility::ThreadPoolExecutor m_workers;
        std::jthread m_control_worker;
    };

public:
    MachinePool(size_t idle_target, size_t max_concurrency,
                std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> machine_spawner,
                utility::ShutdownSignal shutdown_signal)
            : m_impl{std::make_unique<Impl>(idle_target, max_concurrency, std::move(machine_spawner),
                                            std::move(shutdown_signal))} {}

    ~MachinePool() = default;
    MachinePool(MachinePool&&) noexcept = default;
    MachinePool& operator=(MachinePool&&) noexcept = default;

    std::expected<std::shared_ptr<Machine>, GenericError> acquire(std::chrono::milliseconds timeout) {
        return m_impl->acquire(timeout);
    }

    void activate(std::shared_ptr<Machine> machine) { m_impl->activate(std::move(machine)); }
    void deactivate(std::shared_ptr<Machine> machine) { m_impl->deactivate(std::move(machine)); }
    void release(std::shared_ptr<Machine> machine) { m_impl->release(std::move(machine)); }
    void start() { m_impl->start(); }
    void stop() { m_impl->stop(); }

    void set_stats_callback(std::move_only_function<void(const MachinePoolStats&) noexcept> cb) noexcept {
        m_impl->set_stats_callback(std::move(cb));
    }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class MachineManager {
public:
    virtual ~MachineManager() = default;
    virtual std::expected<std::unique_ptr<Machine>, GenericError> spawn(const Machine::Info& info,
                                                                        const std::string& serialized_pool_details,
                                                                        const std::string& serialized_template_details,
                                                                        const std::filesystem::path& config_dir) = 0;
};

class MachineManagerFactory {
public:
    virtual ~MachineManagerFactory() = default;
    virtual std::unique_ptr<MachineManager> create() = 0;
};

class MachineManagerFactorySelector final {
public:
    static std::expected<std::unique_ptr<MachineManagerFactory>, GenericError> get_factory(const std::string& name) {
        if (name == "libvirt") {
            return std::make_unique<LibvirtMachineManagerFactory>();
        }
        return std::unexpected{GenericError{std::format("Invalid machine manager factory name: {}", name)}};
    }
};

std::string Machine::make_temp_path(const std::string& sub_path) const {
    const auto* const delimiter{utility::string_compare_ci(info().os, "windows") == 0 ? "\\" : "/"};
    auto result{utility::string_replace(
        utility::string_from_u8string(
            (std::filesystem::path{info().temp_dir} / utility::u8string_from_string(sub_path)).generic_u8string()),
        "/", delimiter)};
    return result;
}

} // namespace ls_gitea_runner
