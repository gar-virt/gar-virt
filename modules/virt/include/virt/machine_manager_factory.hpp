#pragma once

#include "machine_manager.hpp"

#include <memory>

namespace ls_gitea_runner {

class MachineManagerFactory {
public:
    virtual ~MachineManagerFactory() = default;
    virtual std::unique_ptr<MachineManager> create() = 0;
};

} // namespace ls_gitea_runner
