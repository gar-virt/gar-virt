#include "machine_manager_factory.hpp"
#include "machine_manager.hpp"

namespace ls_gitea_runner {

DockerMachineManagerFactory::~DockerMachineManagerFactory() {}

std::unique_ptr<MachineManager> DockerMachineManagerFactory::create() {
    return std::make_unique<DockerMachineManager>();
}

} // namespace ls_gitea_runner
