#pragma once

#include "level.hpp"

#include <cstdlib>
#include <string_view>

namespace ls_gitea_runner::utility {

enum class LogLevel {
    none,
    error,
    warning,
    info,
    debug,
};

constexpr std::string_view get_log_level_name(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::none:
        return "NONE";
    case LogLevel::error:
        return "ERROR";
    case LogLevel::warning:
        return "WARN";
    case LogLevel::info:
        return "INFO";
    case LogLevel::debug:
        return "DEBUG";
    }
    std::abort();
}

} // namespace ls_gitea_runner::utility
