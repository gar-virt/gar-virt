#include "machine_manager_factory_selector.hpp"
#include "docker_machine_manager_factory.hpp"

namespace ls_gitea_runner {

std::expected<std::unique_ptr<machine_manager_factory>, generic_error>
machine_manager_factory_selector::get_factory(const std::string& name) {
    if (name == "docker") {
        return std::make_unique<docker_machine_manager_factory>();
    }
    return std::unexpected{generic_error{std::format("Invalid machine manager factory name: {}", name)}};
}

} // namespace ls_gitea_runner
