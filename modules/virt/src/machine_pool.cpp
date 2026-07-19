#include <virt/machine_pool.hpp>

#include <utility/algorithm.hpp>
#include <utility/defer.hpp>
#include <utility/error.hpp>
#include <utility/log/global_logger.hpp>
#include <utility/thread_pool_executor.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

namespace ls_gitea_runner {

class MachinePool::Impl final {
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
         std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>() noexcept> machine_spawner,
         utility::ShutdownSignal shutdown_signal)
            : m_shutdown_signal{std::move(shutdown_signal)}, m_idle_target{idle_target},
              m_max_concurrency{max_concurrency}, m_machine_spawner{std::move(machine_spawner)},
              m_workers{0, max_concurrency} {}

    ~Impl() { stop(); }

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;

    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

    std::expected<std::shared_ptr<Machine>, GenericError> acquire(std::chrono::milliseconds timeout) noexcept {
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

    void activate(std::shared_ptr<Machine> /*machine*/) noexcept {
        std::unique_lock lock{m_mutex};
        ++m_machine_counters.active;
        check_stats(lock);
        lock.unlock();
        m_control_cv.notify_one();
    }

    void deactivate(std::shared_ptr<Machine> machine) noexcept {
        std::unique_lock lock{m_mutex};
        deactivate_internal(machine);
    }

    void release(std::shared_ptr<Machine> machine) noexcept {
        machine.reset();
        std::unique_lock lock{m_mutex};
        --m_machine_counters.acquired;
        check_stats(lock);
        lock.unlock();
        m_control_cv.notify_one();
    }

    void start() {
        std::unique_lock lock{m_mutex};
        m_control_worker = std::jthread{[this] { control_loop(); }};
    }

    void stop() noexcept {
        {
            std::unique_lock lock{m_mutex};
            stop_internal(lock);
        }
        if (m_control_worker.joinable()) {
            m_control_worker.join();
        }
        m_workers.stop();
    }

    void set_stats_callback(std::move_only_function<void(MachinePoolStats) noexcept> cb) noexcept {
        std::scoped_lock lock{m_mutex};
        m_stats_cb = std::move(cb);
    }

private:
    void control_loop() noexcept {
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

    void deactivate_internal(std::shared_ptr<Machine> /*machine*/) noexcept { --m_machine_counters.active; }

    void stop_internal(std::unique_lock<std::mutex>& acquired_lock) noexcept {
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
                std::shared_ptr<Machine> machine{*std::move(machine_res)};
                m_idle_machines.emplace(machine);
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

    void check_stats(std::unique_lock<std::mutex>& acquired_lock) noexcept {
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

    void report_stats(std::unique_lock<std::mutex>& acquired_lock) noexcept {
        if (!m_stats_cb) {
            return;
        }
        auto snapshot{m_stats};
        acquired_lock.unlock();
        try {
            m_stats_cb(std::move(snapshot));
        } catch (...) {
            // Ignore exceptions in callback
        }
        acquired_lock.lock();
    }

    bool should_stop() const noexcept { return m_stop || m_shutdown_signal.is_signalled(); }

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
    std::move_only_function<void(MachinePoolStats) noexcept> m_stats_cb;
    std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>() noexcept> m_machine_spawner;
    utility::ThreadPoolExecutor m_workers;
    std::jthread m_control_worker;
};

MachinePool::MachinePool(
    size_t idle_target, size_t max_concurrency,
    std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>() noexcept> machine_spawner,
    utility::ShutdownSignal shutdown_signal)
        : m_impl{std::make_unique<Impl>(idle_target, max_concurrency, std::move(machine_spawner),
                                        std::move(shutdown_signal))} {}

MachinePool::~MachinePool() = default;

MachinePool::MachinePool(MachinePool&&) = default;

MachinePool& MachinePool::operator=(MachinePool&&) = default;

std::expected<std::shared_ptr<Machine>, GenericError> MachinePool::acquire(std::chrono::milliseconds timeout) noexcept {
    return m_impl->acquire(timeout);
}

void MachinePool::activate(std::shared_ptr<Machine> machine) noexcept { m_impl->activate(std::move(machine)); }
void MachinePool::deactivate(std::shared_ptr<Machine> machine) noexcept { m_impl->deactivate(std::move(machine)); }
void MachinePool::release(std::shared_ptr<Machine> machine) noexcept { m_impl->release(std::move(machine)); }
void MachinePool::start() { m_impl->start(); }
void MachinePool::stop() { m_impl->stop(); }

void MachinePool::set_stats_callback(std::move_only_function<void(MachinePoolStats) noexcept> cb) noexcept {
    m_impl->set_stats_callback(std::move(cb));
}

} // namespace ls_gitea_runner
