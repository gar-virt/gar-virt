#include <utility/env.hpp>

#include <cstdlib>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdlib.h>
#include <string>

#ifdef _WIN32
    #include <utility/string.hpp>

    #include <vector>

    #include <windows.h>
#endif

namespace ls_gitea_runner::utility {
namespace {
std::shared_mutex mutex;
}

std::optional<std::string> getenv(const std::string& name) {
    const std::shared_lock lock{mutex};
#ifdef _WIN32
    const auto name_w{widen_string(name)};
    std::vector<wchar_t> value_w;
    while (true) {
        const auto returned{::GetEnvironmentVariableW(name_w.c_str(), value_w.data(), value_w.size())};
        // Read the documentation of GetEnvironmentVariable() very carefully
        if (returned == 0) {
            if (::GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
                return std::nullopt;
            }
            // If the buffer is non-empty then we must have set it in the previous iteration to the size
            // needed including the null character, i.e. enough for an empty string.
            if (value_w.size() == 1) {
                return std::string{};
            }
            // Treat any undocumented error as not-found.
            return std::nullopt;
        }
        if (returned < value_w.size()) {
            return narrow_string(value_w.data(), returned);
        }
        value_w.resize(returned);
    }
#else
    if (const auto* value{std::getenv(name.c_str())}) { // NOLINT(concurrency-mt-unsafe)
        return value;
    }
    return std::nullopt;
#endif
}

void setenv(const std::string& name, const std::string& value) {
    const std::unique_lock lock{mutex};
#ifdef _WIN32
    const auto name_w{widen_string(name)};
    const auto value_w{widen_string(value)};
    ::SetEnvironmentVariableW(name_w.c_str(), value_w.c_str());
#else
    ::setenv(name.c_str(), value.c_str(), 1); // NOLINT(concurrency-mt-unsafe)
#endif
}

void unsetenv(const std::string& name) {
    const std::unique_lock lock{mutex};
#ifdef _WIN32
    const auto wname{widen_string(name)};
    ::SetEnvironmentVariableW(wname.c_str(), nullptr);
#else
    ::unsetenv(name.c_str()); // NOLINT(concurrency-mt-unsafe)
#endif
}

} // namespace ls_gitea_runner::utility
