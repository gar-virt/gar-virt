#pragma once

#include <optional>
#include <string>

namespace utility {

auto getenv(const std::string &name) -> std::optional<std::string>;
auto setenv(const std::string &name, const std::string &value) -> void;

} // namespace utility
