#pragma once

#include <utility/error.hpp>

#include <expected>

namespace gv {

template <typename T> using Result = std::expected<T, Error>;

} // namespace gv
