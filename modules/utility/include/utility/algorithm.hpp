#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace ls_gitea_runner::utility {

// Convert unsigned integral value to (un)signed
template <typename R, typename T>
typename std::enable_if<(std::is_signed<R>::value && !std::is_signed<T>::value) ||
                            (std::is_signed<R>::value == std::is_signed<T>::value),
                        R>::type
safe_cast_int(T value) {
    using r_limits = std::numeric_limits<R>;
    if (static_cast<uintmax_t>(value) > static_cast<uintmax_t>(r_limits::max())) {
        throw std::invalid_argument{"value > result max"};
    }
    return static_cast<R>(value);
}

// Convert signed integral value to unsigned
template <typename R, typename T>
typename std::enable_if<!std::is_signed<R>::value && std::is_signed<T>::value, R>::type safe_cast_int(T value) {
    using r_limits = std::numeric_limits<R>;
    if (value < 0) {
        throw std::invalid_argument{"value < 0"};
    }
    if (static_cast<uintmax_t>(value) > static_cast<uintmax_t>(r_limits::max())) {
        throw std::invalid_argument{"value > result max"};
    }
    return static_cast<R>(value);
}

} // namespace ls_gitea_runner::utility
