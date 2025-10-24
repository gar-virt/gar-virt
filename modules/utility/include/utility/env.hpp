#pragma once

#include <optional>
#include <string>

namespace ls_gitea_runner::utility {

std::optional<std::string> getenv(const std::string& name);
void setenv(const std::string& name, const std::string& value);
void unsetenv(const std::string& name);

} // namespace ls_gitea_runner::utility
