#pragma once

#include <optional>
#include <string>

namespace utility {

std::optional<std::string> getenv(const std::string& name);
void setenv(const std::string& name, const std::string& value);

} // namespace utility
