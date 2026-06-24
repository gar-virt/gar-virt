#include "virt/machine_manager_factory_selector.hpp"
#include "libvirt/machine_manager_factory.hpp"

namespace ls_gitea_runner {

std::expected<std::unique_ptr<MachineManagerFactory>, GenericError>
MachineManagerFactorySelector::get_factory(const std::string& name) {
    if (name == "libvirt") {
        return std::make_unique<LibvirtMachineManagerFactory>();
    }
    return std::unexpected{GenericError{std::format("Invalid machine manager factory name: {}", name)}};
}

} // namespace ls_gitea_runner
