#include <virt/machine_pool.hpp>

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
#include <thread>
#include <unordered_set>

namespace ls_gitea_runner {

class MachinePool::Impl final {
public:
    Impl(size_t idle_target, size_t max_concurrency,
         std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> machine_spawner)
            : m_idle_target{idle_target}, m_max_concurrency{max_concurrency},
              m_machine_spawner{std::move(machine_spawner)}, m_workers{0, max_concurrency} {}

    ~Impl() { stop(); }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    std::expected<std::shared_ptr<Machine>, GenericError> acquire(std::chrono::milliseconds timeout) noexcept {
        using namespace std::chrono_literals;
        std::unique_lock lock{m_mutex};
        ++m_waiting_to_acquire;
        check_stats(lock);
        m_control_cv.notify_one();
        const auto timed_out{!m_idle_cv.wait_for(lock, timeout, [this] { return m_stop || !m_idle_machines.empty(); })};
        --m_waiting_to_acquire;
        m_control_cv.notify_one();
        if (m_stop) {
            return std::unexpected{GenericError{"Shutting down machine pool"}};
        }
        if (timed_out) {
            return std::unexpected{GenericError{"Timed out while waiting to acquire machine"}};
        }
        auto machine{std::move(m_idle_machines.front())};
        m_idle_machines.pop();
        m_active_machines.emplace(machine);
        check_stats(lock);
        m_control_cv.notify_one();
        return machine;
    }

    void release(std::shared_ptr<Machine> machine) noexcept {
        {
            std::unique_lock lock{m_mutex};
            m_active_machines.erase(machine);
            check_stats(lock);
        }
        m_control_cv.notify_one();
    }

    void start() {
        m_control_worker = std::jthread{[this] { control_loop(); }};
    }

    void set_stats_callback(std::move_only_function<void(MachinePoolStats)> cb) noexcept {
        std::scoped_lock lock{m_mutex};
        m_stats_cb = std::move(cb);
    }

private:
    void stop() noexcept {
        std::jthread control_worker;
        {
            std::scoped_lock lock{m_mutex};
            m_stop = true;
            control_worker = std::move(m_control_worker);
        }
        m_control_cv.notify_all();
        m_idle_cv.notify_all();
        if (control_worker.joinable()) {
            control_worker.join();
        }
        m_workers.stop();
    }

    void add_spawner(std::unique_lock<std::mutex>& acquired_lock) {
        ++m_warmup_count;
        check_stats(acquired_lock);
        acquired_lock.unlock();
        m_workers.put([this] { spawn_one(); });
        acquired_lock.lock();
    }

    void spawn_one() {
        bool ok{};
        {
            // FIXME: Need to back off if this keeps failing, otherwise control loop will run hot
            auto machine_res{m_machine_spawner()};
            std::unique_lock lock{m_mutex};
            --m_warmup_count;
            if (machine_res) {
                ok = true;
                m_idle_machines.emplace(*std::move(machine_res));
            } else {
                global_logger().error("Failed to spawn machine: {}", machine_res.error().what());
            }
            check_stats(lock);
        }
        if (ok) {
            m_idle_cv.notify_one();
        }
        m_control_cv.notify_one();
    }

    enum class AutoscaleAction { none, upscale, downscale };

    AutoscaleAction calculate_autoscale_action() const noexcept {
        const auto provisioned{m_active_machines.size() + m_idle_machines.size() + m_warmup_count};
        // Upscale: Catch up to idle target, and spawn extra if we have waiters and enough extra capacity.
        if (provisioned < m_idle_target ||
            (provisioned < m_max_concurrency && (m_idle_machines.size() + m_warmup_count) < m_waiting_to_acquire)) {
            return AutoscaleAction::upscale;
        }
        // Downscale: Remove excess idle that nobody is waiting for.
        if (m_idle_machines.size() > m_idle_target && (m_idle_machines.size() - m_idle_target) > m_waiting_to_acquire) {
            return AutoscaleAction::downscale;
        }
        return AutoscaleAction::none;
    }

    void control_loop() noexcept {
        using namespace std::chrono_literals;
        while (true) {
            std::unique_lock lock{m_mutex};
            if (m_stop) {
                return;
            }
            const auto autoscale_action{calculate_autoscale_action()};
            switch (autoscale_action) {
            case AutoscaleAction::upscale: {
                add_spawner(lock);
                continue;
            }
            case AutoscaleAction::downscale: {
                auto excess_machine{std::move(m_idle_machines.front())};
                m_idle_machines.pop();
                check_stats(lock);
                lock.unlock();
                // Excess machine is destroyed here
                continue;
            }
            case AutoscaleAction::none:
                // Nothing to do
                break;
            }
            m_control_cv.wait_for(lock, 30s,
                                  [this] { return m_stop || calculate_autoscale_action() != AutoscaleAction::none; });
        }
    }

    void check_stats(std::unique_lock<std::mutex>& acquired_lock) {
        const MachinePoolStats temp_stats{
            .active = m_active_machines.size(),
            .idle = m_idle_machines.size(),
            .warming = m_warmup_count,
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
        auto snapshot{m_stats};
        acquired_lock.unlock();
        try {
            m_stats_cb(std::move(snapshot));
        } catch (...) {
            // Ignore exceptions in callback
        }
        acquired_lock.lock();
    }

    size_t m_idle_target{};
    size_t m_max_concurrency{};
    size_t m_warmup_count{};
    size_t m_waiting_to_acquire{};
    bool m_stop{};
    std::queue<std::shared_ptr<Machine>> m_idle_machines;
    std::unordered_set<std::shared_ptr<Machine>> m_active_machines;
    mutable std::mutex m_mutex;
    std::condition_variable m_idle_cv;
    std::condition_variable m_control_cv;
    MachinePoolStats m_stats;
    std::move_only_function<void(MachinePoolStats)> m_stats_cb;
    std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> m_machine_spawner;
    utility::ThreadPoolExecutor m_workers;
    std::jthread m_control_worker;
};

MachinePool::MachinePool(
    size_t idle_target, size_t max_concurrency,
    std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> machine_spawner)
        : m_impl{std::make_unique<Impl>(idle_target, max_concurrency, std::move(machine_spawner))} {}

MachinePool::~MachinePool() = default;

MachinePool::MachinePool(MachinePool&&) = default;

MachinePool& MachinePool::operator=(MachinePool&&) = default;

std::expected<std::shared_ptr<Machine>, GenericError> MachinePool::acquire(std::chrono::milliseconds timeout) noexcept {
    return m_impl->acquire(timeout);
}

void MachinePool::release(std::shared_ptr<Machine> machine) noexcept { return m_impl->release(std::move(machine)); }

void MachinePool::start() { m_impl->start(); }

void MachinePool::set_stats_callback(std::move_only_function<void(MachinePoolStats)> cb) noexcept {
    m_impl->set_stats_callback(std::move(cb));
}

} // namespace ls_gitea_runner
