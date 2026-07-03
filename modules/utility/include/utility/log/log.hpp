#pragma once

#include <utility/datetime.hpp>

#include <format>
#include <string_view>

namespace ls_gitea_runner::utility {

enum class LogLevel {
    none,
    error,
    warning,
    info,
    verbose,
};

class Logger {
public:
    virtual ~Logger() = default;

    constexpr Logger& set_level(LogLevel level) noexcept {
        m_level = level;
        return *this;
    }

    template <typename... Args>
    Logger& log(LogLevel level, std::format_string<Args...> format, Args&&... args) noexcept {
        auto level_int{static_cast<std::underlying_type_t<LogLevel>>(level)};
        auto current_level_int{static_cast<std::underlying_type_t<LogLevel>>(m_level)};

        if (current_level_int < level_int) {
            return *this;
        }

        const auto date{format_date_for_display(utc_to_local_date(utc_date()))};
        log_impl(level, std::format("[{}] {}\n", date, std::format(format, std::forward<Args>(args)...)));

        return *this;
    }

    template <typename... Args> Logger& error(std::format_string<Args...> format, Args&&... args) noexcept {
        return log(LogLevel::error, std::move(format), std::forward<Args>(args)...);
    }

    template <typename... Args> Logger& warning(std::format_string<Args...> format, Args&&... args) noexcept {
        return log(LogLevel::warning, std::move(format), std::forward<Args>(args)...);
    }

    template <typename... Args> Logger& info(std::format_string<Args...> format, Args&&... args) noexcept {
        return log(LogLevel::info, std::move(format), std::forward<Args>(args)...);
    }

    template <typename... Args> Logger& verbose(std::format_string<Args...> format, Args&&... args) noexcept {
        return log(LogLevel::verbose, std::move(format), std::forward<Args>(args)...);
    }

protected:
    virtual void log_impl(LogLevel level, std::string_view s) noexcept = 0;

    static constexpr bool is_error_like(LogLevel level) noexcept {
        return level == LogLevel::error || level == LogLevel::warning;
    }

private:
    LogLevel m_level{LogLevel::none};
};

} // namespace ls_gitea_runner::utility
