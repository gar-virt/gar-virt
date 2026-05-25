#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

namespace ls_gitea_runner::utility {

class ThreadPoolExecutor final {
    using ULock = std::unique_lock<std::mutex>;
    using Lock = std::lock_guard<std::mutex>;

public:
    using TaskFn = std::function<void()>;

    ThreadPoolExecutor(std::optional<size_t> thread_count = std::nullopt) {
        add_workers(thread_count.value_or(std::thread::hardware_concurrency()));
    }

    ~ThreadPoolExecutor() {
        {
            Lock lock{m_mutex};
            m_stop = true;
        }
        m_cv.notify_all();
        for (auto& thread : m_workers) {
            thread.join();
        }
    }

    void cancel() {
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
            m_queue.push(std::bind(std::forward<F>(work), std::forward<Args>(args)...));
        }
        m_cv.notify_one();
    }

    size_t get_thread_count() const {
        Lock lock{m_mutex};
        return m_workers.size();
    }

private:
    void worker_fn() {
        while (true) {
            TaskFn task;
            {
                ULock lock{m_mutex};
                m_cv.wait(lock, [this] { return m_cancel || m_stop || !m_queue.empty(); });
                if (m_cancel || (m_stop && m_queue.empty())) {
                    return;
                }
                if (m_queue.empty()) {
                    continue;
                }
                task = std::move(m_queue.front());
                m_queue.pop();
            }
            try {
                task();
            } catch (...) {
                // Ignore
            }
        }
    }

    void add_workers(size_t count) {
        Lock lock{m_mutex};
        while (m_workers.size() < count) {
            add_worker();
        }
    }

    void add_worker() {
        m_workers.emplace_back([this] { worker_fn(); });
    }

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop{};
    bool m_cancel{};
    std::queue<TaskFn> m_queue;
    std::vector<std::jthread> m_workers;
};

} // namespace ls_gitea_runner::utility
