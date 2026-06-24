#pragma once

#include "../machine/machine_manager_factory.hpp"

namespace ls_gitea_runner {

class LibvirtMachineManagerFactory final : public MachineManagerFactory {
public:
    ~LibvirtMachineManagerFactory();
    std::unique_ptr<MachineManager> create() override;
};

} // namespace ls_gitea_runner
