#include "machine_manager_factory_selector.hpp"
#include "docker_machine_manager_factory.hpp"

namespace ls_gitea_runner {

std::expected<std::unique_ptr<MachineManagerFactory>, GenericError>
MachineManagerFactorySelector::get_factory(const std::string& name) {
    if (name == "docker") {
        return std::make_unique<DockerMachineManagerFactory>();
    }
    return std::unexpected{GenericError{std::format("Invalid machine manager factory name: {}", name)}};
}

} // namespace ls_gitea_runner
