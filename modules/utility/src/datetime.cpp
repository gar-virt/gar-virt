#include <utility/datetime.hpp>
#include <utility/error.hpp>

#include <concepts>
#include <cstdio>
#include <ctime>
#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <time.h>

namespace ls_gitea_runner::utility {
namespace {
constexpr std::string_view utc_date_time_format{"yyyy-mm-ddThh:mm:ssZ"};
constexpr std::string_view local_date_time_format{"yyyy-mm-dd hh:mm:ss"};
} // namespace

std::tm utc_date() {
    auto time{std::time(nullptr)};
    std::tm result{};
#ifdef _WIN32
    ::gmtime_s(&result, &time);
#else
    ::gmtime_r(&time, &result);
#endif
    return result;
}

std::string utc_date_string(const std::tm& time) {
    std::string result(utc_date_time_format.size(), '\0');
    if (std::strftime(result.data(), result.size() + 1, "%FT%TZ", &time) == 0) {
        throw std::runtime_error{"Failed to format date"};
    }
    return result;
}

std::expected<std::tm, GenericError> parse_utc_date_string(const std::string& from) {
    std::tm time{};
    time.tm_isdst = -1;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    auto parsed{std::sscanf(from.c_str(), "%4d-%2d-%2dT%2d:%2d:%2dZ", &time.tm_year, &time.tm_mon, &time.tm_mday,
                            &time.tm_hour, &time.tm_min, &time.tm_sec)};
    constexpr static int expected_parts{6};
    if (parsed != expected_parts) {
        return std::unexpected{GenericError{"Failed to parse date time string"}};
    }
    constexpr static int epoch_year{1900};
    time.tm_year -= epoch_year;
    --time.tm_mon;
    return time;
}

std::tm utc_to_local_date(const std::tm& from) {
    static_assert(std::same_as<std::tm, ::tm>);
    std::tm from_{from};
    std::tm result{};
#ifdef _WIN32
    auto t{::_mkgmtime(&from_)};
    ::localtime_s(&result, &t);
#else
    auto t{::timegm(&from_)};
    ::localtime_r(&t, &result);
#endif
    return result;
}

std::string format_date_for_display(const std::tm& time) {
    std::string result(local_date_time_format.size(), '\0');
    if (std::strftime(result.data(), result.size() + 1, "%F %T", &time) == 0) {
        throw std::runtime_error{"Failed to format date"};
    }
    return result;
}

} // namespace ls_gitea_runner::utility
