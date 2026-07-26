#include "machine_manager_factory.hpp"
#include "machine_manager.hpp"

namespace gv {

LibvirtMachineManagerFactory::~LibvirtMachineManagerFactory() = default;

std::unique_ptr<MachineManager> LibvirtMachineManagerFactory::create() {
    return std::make_unique<LibvirtMachineManager>();
}

} // namespace gv
