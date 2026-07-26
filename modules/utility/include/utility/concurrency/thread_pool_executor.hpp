#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace gv::utility {

class ThreadPoolExecutor final {
public:
    using TaskFn = std::move_only_function<void()>;

    ThreadPoolExecutor(const std::optional<size_t>& min_thread_count = std::nullopt,
                       const std::optional<size_t>& max_thread_count = std::nullopt);
    ~ThreadPoolExecutor();

    ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor(ThreadPoolExecutor&&) noexcept;

    ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor& operator=(ThreadPoolExecutor&&) noexcept;

    void stop();
    void cancel();

    template <typename F, typename... Args> void put(F&& work, Args&&... args) {
        put_impl([work = std::forward<F>(work), ... args = std::forward<Args>(args)]() mutable {
            std::invoke(std::move(work), std::move(args)...);
        });
    }

private:
    void put_impl(TaskFn work);

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace gv::utility
