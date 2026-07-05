#pragma once

#include <functional>
#include <utility>

namespace ls_gitea_runner::utility {

class Deferred {
public:
    template <typename T> Deferred(T fn) : m_fn{std::move(fn)} {};

    ~Deferred() {
        if (m_fn) {
            m_fn();
        }
    }

    Deferred(const Deferred&) = delete;
    Deferred(Deferred&& other) noexcept : m_fn{std::exchange(other.m_fn, {})} {}

    Deferred& operator=(const Deferred&) = delete;

    Deferred& operator=(Deferred&& other) noexcept {
        if (this != &other) {
            m_fn = std::exchange(other.m_fn, {});
        }
        return *this;
    }

private:
    std::function<void()> m_fn;
};

} // namespace ls_gitea_runner::utility
