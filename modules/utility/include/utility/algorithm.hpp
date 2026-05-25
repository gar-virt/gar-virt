#pragma once

#include <limits>
#include <stdexcept>
#include <type_traits>

namespace ls_gitea_runner::utility {

/// Safely convert integral value from one type to another type where both are
/// either signed or unsigned.
template<typename R, typename T>
typename std::enable_if<
    std::is_signed<R>::value == std::is_signed<T>::value &&
        std::is_integral<T>::value && std::is_integral<R>::value &&
        !std::is_same<T, bool>::value && !std::is_same<R, bool>::value,
    R>::type
safe_cast_int(T value) {
    using wider_t =
        typename std::conditional<sizeof(T) >= sizeof(R), T, R>::type;
    if (static_cast<wider_t>(value) <
        static_cast<wider_t>(std::numeric_limits<R>::min())) {
        throw std::invalid_argument{"value < result min"};
    }
    if (static_cast<wider_t>(value) >
        static_cast<wider_t>(std::numeric_limits<R>::max())) {
        throw std::invalid_argument{"value > result max"};
    }
    return static_cast<R>(value);
}

/// Safely convert integral value from signed type to unsigned type.
template<typename R, typename T>
typename std::enable_if<
    !std::is_signed<R>::value && std::is_signed<T>::value &&
        std::is_integral<T>::value && std::is_integral<R>::value &&
        !std::is_same<T, bool>::value && !std::is_same<R, bool>::value,
    R>::type
safe_cast_int(T value) {
    using wider_unsigned_t = typename std::conditional<
        sizeof(T) >= sizeof(R), typename std::make_unsigned<T>::type, R>::type;
    if (value < 0) {
        throw std::invalid_argument{"value < 0"};
    }
    if (static_cast<wider_unsigned_t>(value) >
        static_cast<wider_unsigned_t>(std::numeric_limits<R>::max())) {
        throw std::invalid_argument{"value > result max"};
    }
    return static_cast<R>(value);
}

/// Safely convert integral value from unsigned type to signed type.
template<typename R, typename T>
typename std::enable_if<
    std::is_signed<R>::value && !std::is_signed<T>::value &&
        std::is_integral<T>::value && std::is_integral<R>::value &&
        !std::is_same<T, bool>::value && !std::is_same<R, bool>::value,
    R>::type
safe_cast_int(T value) {
    using wider_unsigned_t =
        typename std::conditional<sizeof(T) >= sizeof(R), T,
                                  typename std::make_unsigned<R>::type>::type;
    if (static_cast<wider_unsigned_t>(value) >
        static_cast<wider_unsigned_t>(std::numeric_limits<R>::max())) {
        throw std::invalid_argument{"value > result max"};
    }
    return static_cast<R>(value);
}

} // namespace ls_gitea_runner::utility
