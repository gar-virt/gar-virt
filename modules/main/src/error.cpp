#include "error.hpp"

namespace ls_gitea_runner {

generic_error::generic_error(const std::string& message, std::source_location sloc)
        : runtime_error{message}, m_sloc{sloc} {}

const std::source_location& generic_error::where() const noexcept { return m_sloc; }

} // namespace ls_gitea_runner
