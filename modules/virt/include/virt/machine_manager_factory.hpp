#pragma once

#include "machine_manager.hpp"

#include <memory>

namespace gv::virt {

class MachineManagerFactory {
public:
    virtual ~MachineManagerFactory() = default;
    virtual std::unique_ptr<MachineManager> create() = 0;
};

} // namespace gv::virt
