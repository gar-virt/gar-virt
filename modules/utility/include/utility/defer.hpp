#pragma once

#include <functional>

namespace ls_gitea_runner::utility {

class Deferred {
public:
    template <typename T> Deferred(T&& cb) : m_cb{std::forward<T>(cb)} {}
    ~Deferred() { m_cb(); }

private:
    std::function<void()> m_cb;
};

template <typename Callable> Deferred defer(Callable&& fn) { return Deferred{std::forward<Callable>(fn)}; }

} // namespace ls_gitea_runner::utility
