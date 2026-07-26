#include <utility/error.hpp>

namespace gv {

GenericError::GenericError(const std::string& message, std::source_location sloc)
        : runtime_error{message}, m_sloc{sloc} {}

const std::source_location& GenericError::where() const noexcept { return m_sloc; }

} // namespace gv
