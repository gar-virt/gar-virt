#include <utility/log/global_logger.hpp>
#include <utility/log/log.hpp>
#include <utility/log/stdout.hpp>

namespace ls_gitea_runner {

utility::Logger& global_logger() noexcept {
    static auto global_logger_instance{[] {
        utility::StdOutLogger instance;
        instance.set_level(utility::LogLevel::error);
        return instance;
    }()};
    return global_logger_instance;
}

} // namespace ls_gitea_runner
