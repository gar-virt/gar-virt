#pragma once

#include <utility/result.hpp>

#include <ctime>
#include <expected>
#include <string>

namespace gv::utility {

std::tm utc_date();
std::string utc_date_string(const std::tm& time);
Result<std::tm> parse_utc_date_string(const std::string& from);
std::tm utc_to_local_date(const std::tm& from);
std::string format_date_for_display(const std::tm& time);

} // namespace gv::utility
