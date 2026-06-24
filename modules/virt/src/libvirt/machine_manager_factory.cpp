#include "machine_manager_factory.hpp"
#include "machine_manager.hpp"

namespace ls_gitea_runner {

LibvirtMachineManagerFactory::~LibvirtMachineManagerFactory() {}

std::unique_ptr<MachineManager> LibvirtMachineManagerFactory::create() {
    return std::make_unique<LibvirtMachineManager>();
}

} // namespace ls_gitea_runner
