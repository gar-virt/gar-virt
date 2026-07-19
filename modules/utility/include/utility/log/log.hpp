#pragma once

#include <utility/datetime.hpp>
#include <utility/log/ansi.hpp>
#include <utility/log/level.hpp>

#include <cstdlib>
#include <format>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace ls_gitea_runner::utility {

enum class LogCapability { log_thread };

using LogLine = std::vector<std::variant<ansi::Sequence, std::string>>;

struct LogRequest {
    LogLevel level{};
    LogLine line;
    bool always_flush{};
};

class Logger {
public:
    virtual ~Logger() = default;

    Logger& set_level(LogLevel level) {
        std::scoped_lock lock{*m_mutex};
        m_level = level;
        return *this;
    }

    void set_capability(LogCapability cap, bool enable) {
        std::scoped_lock lock{*m_mutex};
        switch (cap) {
        case LogCapability::log_thread:
            m_log_thread = enable;
            break;
        }
    }

    template <typename... Args>
    Logger& log(LogLevel level, std::format_string<Args...> format, Args&&... args) noexcept {
        try {
            std::scoped_lock lock{*m_mutex};

            const auto level_int{std::to_underlying(level)};
            const auto current_level_int{std::to_underlying(m_level)};

            if (current_level_int < level_int) {
                return *this;
            }

            append_date();
            append_thread();
            append_level(level);
            append_message(std::move(format), std::forward<Args>(args)...);
            flush(level);
        } catch (...) {
            // Ignore
        }

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
    virtual void print_impl(const LogRequest& req) = 0;

    static constexpr bool is_error_like(LogLevel level) noexcept {
        return level == LogLevel::error || level == LogLevel::warning;
    }

private:
    void flush(LogLevel level) {
        print_impl(LogRequest{level, std::move(m_line), always_flush});
        m_line = {};
    }

    template <typename... Args> constexpr void append(Args&&... args) { (append_part(args), ...); }

    template <typename T> constexpr void append_part(T&& part) { m_line.push_back(std::forward<T>(part)); }

    void append_date() {
        append(ansi::Color{240}, format_date_for_display(utc_to_local_date(utc_date())), ansi::Reset{});
    }

    void append_level(LogLevel level) {
        append(" ", color_from_level(level), std::format("{:<5}", get_log_level_name(level)), ansi::Reset{});
    }

    void append_thread() {
        if (m_log_thread) {
            append(" ", ansi::Color{240}, std::format("[{:>15}]", std::this_thread::get_id()), ansi::Reset{});
        }
    }

    template <typename... Args> void append_message(std::format_string<Args...> format, Args&&... args) {
        append(std::format(" {}\n", std::format(format, std::forward<Args>(args)...)));
    }

    constexpr ansi::Color color_from_level(LogLevel level) noexcept {
        switch (level) {
        case LogLevel::none:
            return {7}; // gray
        case LogLevel::error:
            return {9}; // red
        case LogLevel::warning:
            return {11}; // yellow
        case LogLevel::info:
            return {2}; // green
        case LogLevel::debug:
            return {14}; // cyan
        }
        std::abort();
    }

    static constexpr bool always_flush{
#ifdef NDEBUG
        false
#else
        true
#endif
    };

    LogLevel m_level{LogLevel::none};
    bool m_log_thread{};
    std::unique_ptr<std::mutex> m_mutex{std::make_unique<std::mutex>()};
    LogLine m_line;
};

} // namespace ls_gitea_runner::utility
