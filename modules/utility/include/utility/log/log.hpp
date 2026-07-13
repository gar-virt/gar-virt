#pragma once

#include <utility/datetime.hpp>

#include <format>
#include <string_view>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

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

enum class LogCapability { log_thread };

class Logger {
public:
    virtual ~Logger() = default;

    constexpr Logger& set_level(LogLevel level) noexcept {
        std::scoped_lock lock{*m_mutex};
        m_level = level;
        return *this;
    }

    constexpr void set_capability(LogCapability cap, bool enable) noexcept {
        std::scoped_lock lock{*m_mutex};
        switch (cap) {
        case LogCapability::log_thread:
            m_log_thread = enable;
            break;
        }
    }

    template <typename... Args>
    Logger& log(LogLevel level, std::format_string<Args...> format, Args&&... args) noexcept {
        std::scoped_lock lock{*m_mutex};

        const auto level_int{std::to_underlying(level)};
        const auto current_level_int{std::to_underlying(m_level)};

        if (current_level_int < level_int) {
            return *this;
        }

        std::string log_line;
        append_date(log_line);
        append_thread(log_line);
        append_level(log_line, level);
        append_message(log_line, std::move(format), std::forward<Args>(args)...);
        log_impl(level, log_line);
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

    template <typename... Args> Logger& debug(std::format_string<Args...> format, Args&&... args) noexcept {
        return log(LogLevel::debug, std::move(format), std::forward<Args>(args)...);
    }

protected:
    virtual void log_impl(LogLevel level, std::string_view s) noexcept = 0;

    static constexpr bool is_error_like(LogLevel level) noexcept {
        return level == LogLevel::error || level == LogLevel::warning;
    }

private:
    void append_date(std::string& line) noexcept {
        line += std::format("{}", format_date_for_display(utc_to_local_date(utc_date())));
    }

    void append_level(std::string& line, LogLevel level) noexcept {
        line += std::format(" {:<5}", get_log_level_name(level));
    }

    void append_thread(std::string& line) noexcept {
        if (m_log_thread) {
            line += std::format(" [{:>15}]", std::this_thread::get_id());
        }
    }

    template <typename... Args>
    void append_message(std::string& line, std::format_string<Args...> format, Args&&... args) noexcept {
        line += std::format(" {}\n", std::format(format, std::forward<Args>(args)...));
    }

    LogLevel m_level{LogLevel::none};
    bool m_log_thread{};
    std::unique_ptr<std::mutex> m_mutex{std::make_unique<std::mutex>()};
};

} // namespace ls_gitea_runner::utility
