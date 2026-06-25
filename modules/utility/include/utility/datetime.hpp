#pragma once

#include "utility/error.hpp"

#include <ctime>
#include <expected>
#include <string>

namespace ls_gitea_runner::utility {

std::tm utc_date();
std::string utc_date_string(const std::tm& time);
std::expected<std::tm, GenericError> parse_utc_date_string(const std::string& from);
std::tm utc_to_local_date(const std::tm& from);
std::string format_date_for_display(const std::tm& time);

} // namespace ls_gitea_runner::utility
