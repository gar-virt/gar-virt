#pragma once

#include "utility/log/log.hpp"

namespace ls_gitea_runner::utility {

class StdOutLogger : public Logger {
protected:
    void log_impl(LogLevel level, std::string_view s) noexcept override;
};

} // namespace ls_gitea_runner::utility
