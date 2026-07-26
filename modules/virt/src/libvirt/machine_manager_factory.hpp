#pragma once

#include <virt/machine_manager_factory.hpp>

namespace gv::virt {

class LibvirtMachineManagerFactory final : public MachineManagerFactory {
public:
    ~LibvirtMachineManagerFactory();
    std::unique_ptr<MachineManager> create() override;
};

} // namespace gv::virt
