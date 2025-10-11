#include "utility/env.hpp"
#include "utility/string.hpp"

#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace utility {

auto getenv(const std::string& name) -> std::optional<std::string> {
    if (auto value = std::getenv(name.c_str())) {
        return value;
    } else {
        return std::nullopt;
    }
}

auto setenv(const std::string& name, const std::string& value) -> void {
#ifdef _WIN32
    SetEnvironmentVariableW(widen(name).c_str(), widen(value).c_str());
#else
    ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

} // namespace utility
