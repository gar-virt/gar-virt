#pragma once

#include <utility/uuid.hpp>

#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ls_gitea_runner::utility {

class ThreadPoolExecutor final {
    using ULock = std::unique_lock<std::mutex>;
    using Lock = std::lock_guard<std::mutex>;

    struct Worker {
        std::string id;
        std::jthread thread;
    };

public:
    using TaskFn = std::move_only_function<void()>;

    ThreadPoolExecutor(std::optional<size_t> min_thread_count = std::nullopt,
                       std::optional<size_t> max_thread_count = std::nullopt)
            : m_min_thread_count{min_thread_count.value_or(std::thread::hardware_concurrency())},
              m_max_thread_count{max_thread_count.value_or(m_min_thread_count)} {
        add_fixed_workers(m_min_thread_count);
    }

    ~ThreadPoolExecutor() { stop(); }

    void stop() noexcept {
        std::vector<std::jthread> threads;
        {
            Lock lock{m_mutex};
            m_stop = true;
            threads.reserve(m_workers.size());
            for (auto& [id, worker] : m_workers) {
                threads.push_back(std::move(worker.thread));
            }
        }
        m_cv.notify_all();
        for (auto& thread : threads) {
            thread.join();
        }
    }

    void cancel() noexcept {
        {
            Lock lock{m_mutex};
            m_cancel = true;
        }
        m_cv.notify_all();
    }

    template <typename F, typename... Args> void put(F&& work, Args&&... args) {
        {
            Lock lock{m_mutex};
            if (m_stop) {
                return;
            }
            m_queue.push([work = std::forward<F>(work), ... args = std::forward<Args>(args)]() mutable {
                std::invoke(std::move(work), std::move(args)...);
            });
            assert(m_busy_count <= m_workers.size());
            const auto free_workers{m_workers.size() - m_busy_count};
            if (m_queue.size() > free_workers && m_workers.size() < m_max_thread_count) {
                add_worker(true);
            }
        }
        m_cv.notify_one();
    }

private:
    void worker_fn(std::string worker_id, bool is_ondemand) {
        while (true) {
            ULock lock{m_mutex};
        dead_workers_loop:
            for (size_t i{m_dead_workers.size()}; i > 0; --i) {
                auto& dead_worker_ref{m_dead_workers.at(i - 1)};
                // This should never happen because dying worker threads exit without coming back here.
                // The check is only here to make it clear that we don't want a deadlock due to a self-join.
                if (dead_worker_ref.id == worker_id) {
                    continue;
                }
                auto dead_worker{std::move(dead_worker_ref)};
                m_dead_workers.erase(std::next(m_dead_workers.begin(), i - 1));
                lock.unlock();
                dead_worker.thread.join();
                lock.lock();
                goto dead_workers_loop;
            }
            m_cv.wait(lock, [this] { return m_cancel || m_stop || !m_queue.empty(); });
            if (m_cancel || (m_stop && m_queue.empty())) {
                return;
            }
            if (m_queue.empty()) {
                continue;
            }
            auto task{std::move(m_queue.front())};
            m_queue.pop();
            ++m_busy_count;
            lock.unlock();
            try {
                task();
            } catch (...) {
                // Ignore
            }
            lock.lock();
            --m_busy_count;
            if (is_ondemand && m_busy_count < m_workers.size() && m_queue.empty()) {
                auto worker{std::move(m_workers.at(worker_id))};
                m_workers.erase(worker_id);
                m_dead_workers.emplace_back(std::move(worker));
                return;
            }
        }
    }

    void add_fixed_workers(size_t count) {
        Lock lock{m_mutex};
        while (m_workers.size() < count) {
            add_worker(false);
        }
    }

    void add_worker(bool is_ondemand) {
        auto worker_id{uuid()};
        m_workers.emplace(worker_id, Worker{
                                         .id = worker_id,
                                         .thread = std::jthread{[=, this] { worker_fn(worker_id, is_ondemand); }},
                                     });
    }

    size_t m_min_thread_count{};
    size_t m_max_thread_count{};
    size_t m_busy_count{};
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop{};
    bool m_cancel{};
    std::queue<TaskFn> m_queue;
    std::unordered_map<std::string, Worker> m_workers;
    std::vector<Worker> m_dead_workers;
};

} // namespace ls_gitea_runner::utility
