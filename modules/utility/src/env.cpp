#include <utility/env.hpp>

#include <cstdlib>

#ifdef _WIN32
    #include <utility/string.hpp>

    #include <vector>

    #include <windows.h>
#endif

namespace ls_gitea_runner::utility {

std::optional<std::string> getenv(const std::string& name) {
#ifdef _WIN32
    const auto name_w{widen_string(name)};
    const auto needed{::GetEnvironmentVariableW(name_w.c_str(), nullptr, 0)};
    if (needed > 0) {
        std::vector<wchar_t> value_w;
        value_w.resize(needed);
        const auto returned{::GetEnvironmentVariableW(name_w.c_str(), value_w.data(), needed)};
        return narrow_string(value_w.data(), value_w.size() - 1);
    }
#else
    if (auto value{std::getenv(name.c_str())}) {
        return value;
    }
#endif
    return std::nullopt;
}

void setenv(const std::string& name, const std::string& value) {
#ifdef _WIN32
    const auto name_w{widen_string(name)};
    const auto value_w{widen_string(value)};
    ::SetEnvironmentVariableW(name_w.c_str(), value_w.c_str());
#else
    ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

void unsetenv(const std::string& name) {
#ifdef _WIN32
    const auto& wname{widen_string(name)};
    ::SetEnvironmentVariableW(wname.c_str(), nullptr);
#else
    ::unsetenv(name.c_str());
#endif
}

} // namespace ls_gitea_runner::utility
