#pragma once

#include "../machine/machine_manager_factory.hpp"

namespace ls_gitea_runner {

class DockerMachineManagerFactory final : public MachineManagerFactory {
public:
    ~DockerMachineManagerFactory();
    std::unique_ptr<MachineManager> create() override;
};

} // namespace ls_gitea_runner
