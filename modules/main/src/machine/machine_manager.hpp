#pragma once

#include "../error.hpp"
#include "machine.hpp"

#include <memory>

namespace ls_gitea_runner {

class machine_manager {
public:
    virtual ~machine_manager() = default;
    virtual std::expected<std::unique_ptr<machine>, generic_error> spawn(machine::info_t info,
                                                                         const std::string& details) = 0;
};

} // namespace ls_gitea_runner
