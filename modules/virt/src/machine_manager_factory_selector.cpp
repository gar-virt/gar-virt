#include <virt/machine_manager_factory_selector.hpp>

#include "libvirt/machine_manager_factory.hpp"

namespace gv {

std::expected<std::unique_ptr<MachineManagerFactory>, Error>
MachineManagerFactorySelector::get_factory(const std::string& name) {
    if (name == "libvirt") {
        return std::make_unique<LibvirtMachineManagerFactory>();
    }
    return std::unexpected{Error{std::format("Invalid machine manager factory name: {}", name)}};
}

} // namespace gv
