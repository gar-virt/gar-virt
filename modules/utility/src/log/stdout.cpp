#include <utility/log/stdout.hpp>

#include <iostream>

namespace ls_gitea_runner::utility {

void StdOutLogger::log_impl(LogLevel level, std::string_view s) noexcept {
    std::ostream* file{is_error_like(level) ? &std::cerr : &std::cout};
    file->write(s.data(), s.size());
    file->flush();
}

} // namespace ls_gitea_runner::utility
