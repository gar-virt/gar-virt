module;

#include <source_location>
#include <stdexcept>
#include <string>

export module utility.misc:error;

export namespace ls_gitea_runner {

class GenericError : public std::runtime_error {
public:
    GenericError(const std::string& message, std::source_location sloc = {}) : runtime_error{message}, m_sloc{sloc} {}
    const std::source_location& where() const noexcept { return m_sloc; }

private:
    std::source_location m_sloc;
};

} // namespace ls_gitea_runner
