#pragma once

#include "../error.hpp"
#include "machine.hpp"
#include "machine_pool_details.hpp"

#include <memory>

namespace ls_gitea_runner {

class MachineManager {
public:
    virtual ~MachineManager() = default;
    virtual std::expected<std::unique_ptr<Machine>, GenericError> spawn(Machine::Info info,
                                                                        MachinePoolDetails details) = 0;
};

} // namespace ls_gitea_runner
