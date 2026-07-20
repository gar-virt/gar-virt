#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>

namespace ls_gitea_runner::utility {

template <typename T>
concept contiguous_byte_container =
    requires {
        typename T::iterator;
        typename T::value_type;
    } && std::contiguous_iterator<typename T::iterator> &&
    (std::same_as<typename T::value_type, std::byte> ||
     (std::integral<typename T::value_type> && (sizeof(typename T::value_type) == 1)));

} // namespace ls_gitea_runner::utility
