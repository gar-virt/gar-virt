#pragma once

#include <source_location>
#include <stdexcept>
#include <string>

namespace ls_gitea_runner {

class generic_error : public std::runtime_error {
public:
    generic_error(const std::string& message, std::source_location sloc = std::source_location::current());

    const std::source_location& where() const noexcept;

private:
    std::source_location m_sloc;
};

} // namespace ls_gitea_runner
