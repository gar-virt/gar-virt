#pragma once

#include <functional>

namespace ls_gitea_runner::utility {

class deferred_t {
public:
    template <typename T> deferred_t(T&& cb) : m_cb{std::forward<T>(cb)} {}
    ~deferred_t() { m_cb(); }

private:
    std::function<void()> m_cb;
};

template <typename Callable> deferred_t defer(Callable&& fn) { return deferred_t{std::forward<Callable>(fn)}; }

} // namespace ls_gitea_runner::utility
