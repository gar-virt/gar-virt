#pragma once

#include <source_location>
#include <stdexcept>
#include <string>

namespace gv {

class Error : public std::runtime_error {
public:
    Error(const std::string& message, std::source_location sloc = std::source_location::current());

    const std::source_location& where() const noexcept;

private:
    std::source_location m_sloc;
};

} // namespace gv
