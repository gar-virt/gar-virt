#include <utility/datetime.hpp>
#include <utility/error.hpp>

#include <cstdio>
#include <ctime>
#include <expected>

namespace ls_gitea_runner::utility {

std::tm utc_date() {
    auto time{std::time({})};
    return *std::gmtime(&time);
}

std::string utc_date_string(const std::tm& time) {
    char timeString[std::size("yyyy-mm-ddThh:mm:ssZ")]{};
    std::strftime(std::data(timeString), std::size(timeString), "%FT%TZ", &time);
    return timeString;
}

std::expected<std::tm, GenericError> parse_utc_date_string(const std::string& from) {
    std::tm time{};
    time.tm_isdst = -1;
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
    std::tm from_{from};
#ifdef _WIN32
    auto t{::_mkgmtime(&from_)};
#else
    auto t{::timegm(&from_)};
#endif
    auto* local{std::localtime(&t)};
    return *local;
}

std::string format_date_for_display(const std::tm& time) {
    char timeString[std::size("yyyy-mm-dd hh:mm:ss")]{};
    std::strftime(std::data(timeString), std::size(timeString), "%F %T", &time);
    return timeString;
}

} // namespace ls_gitea_runner::utility
