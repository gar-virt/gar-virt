#include <virt/machine_pool.hpp>

#include <utility/error.hpp>
#include <utility/log/global_logger.hpp>

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
    Impl(size_t capacity,
         std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> machine_spawner)
            : m_capacity{capacity}, m_machine_spawner{std::move(machine_spawner)} {}

    ~Impl() { stop(); }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    std::expected<std::shared_ptr<Machine>, GenericError> acquire(std::chrono::milliseconds timeout) noexcept {
        using namespace std::chrono_literals;
        // TODO: timeout
        std::unique_lock lock{m_mutex};
        while (true) {
            m_cv.wait_for(lock, 500ms, [this] { return m_stop || !m_idle_machines.empty(); });
            if (m_stop) {
                return std::unexpected{GenericError{"Shutting down machine pool"}};
            }
            if (m_idle_machines.empty()) {
                continue;
            }
            auto machine{std::move(m_idle_machines.front())};
            m_idle_machines.pop();
            m_active_machines.emplace(machine);
            check_stats(lock);
            return machine;
        }
    }

    void release(std::shared_ptr<Machine> machine) noexcept {
        {
            std::unique_lock lock{m_mutex};
            m_active_machines.erase(machine);
            check_stats(lock);
        }
        m_cv.notify_all();
    }

    void start() {
        for (size_t i{}; i < m_capacity; ++i) {
            m_workers.emplace_back([this] { work(); });
        }
    }

    bool at_capacity() const noexcept {
        std::scoped_lock lock{m_mutex};
        return m_active_machines.size() >= m_capacity;
    }

    void set_stats_callback(std::move_only_function<void(MachinePoolStats)> cb) noexcept {
        std::scoped_lock lock{m_mutex};
        m_stats_cb = std::move(cb);
    }

private:
    void stop() noexcept {
        {
            std::scoped_lock lock{m_mutex};
            m_stop = true;
        }
        m_cv.notify_all();
        for (auto& t : m_workers) {
            t.join();
        }
    }

    bool at_capacity_internal() const noexcept {
        return m_active_machines.size() + m_idle_machines.size() + m_warmup_count >= m_capacity;
    }

    void work() noexcept {
        using namespace std::chrono_literals;
        while (true) {
            std::unique_lock lock{m_mutex};
            if (m_stop) {
                return;
            }
            if (at_capacity_internal()) {
                lock.unlock();
                std::this_thread::sleep_for(500ms);
                continue;
            }
            ++m_warmup_count;
            check_stats(lock);
            lock.unlock();
            auto machine_res{m_machine_spawner()};
            if (!machine_res) {
                lock.lock();
                --m_warmup_count;
                global_logger().error("Failed to spawn machine: {}", machine_res.error().what());
                check_stats(lock);
                lock.unlock();
                std::this_thread::sleep_for(5s);
                continue;
            }
            lock.lock();
            m_idle_machines.emplace(*std::move(machine_res));
            --m_warmup_count;
            check_stats(lock);
            lock.unlock();
            m_cv.notify_one();
        }
    }

    void check_stats(std::unique_lock<std::mutex>& acquired_lock) {
        if (m_stats.active == m_active_machines.size() && m_stats.idle == m_idle_machines.size() &&
            m_stats.warming == m_warmup_count) {
            return;
        }
        m_stats.active = m_active_machines.size();
        m_stats.idle = m_idle_machines.size();
        m_stats.warming = m_warmup_count;
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

    size_t m_capacity;
    size_t m_warmup_count{};
    bool m_stop{};
    std::vector<std::jthread> m_workers;
    std::queue<std::shared_ptr<Machine>> m_idle_machines;
    std::unordered_set<std::shared_ptr<Machine>> m_active_machines;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    MachinePoolStats m_stats;
    std::move_only_function<void(MachinePoolStats)> m_stats_cb;
    std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> m_machine_spawner;
};

MachinePool::MachinePool(
    size_t capacity, std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> machine_spawner)
        : m_impl{std::make_unique<Impl>(capacity, std::move(machine_spawner))} {}

MachinePool::~MachinePool() = default;

std::expected<std::shared_ptr<Machine>, GenericError> MachinePool::acquire(std::chrono::milliseconds timeout) noexcept {
    return m_impl->acquire(timeout);
}

void MachinePool::release(std::shared_ptr<Machine> machine) noexcept { return m_impl->release(std::move(machine)); }

void MachinePool::start() { m_impl->start(); }

bool MachinePool::at_capacity() const noexcept { return m_impl->at_capacity(); }

void MachinePool::set_stats_callback(std::move_only_function<void(MachinePoolStats)> cb) noexcept {
    m_impl->set_stats_callback(std::move(cb));
}

} // namespace ls_gitea_runner
