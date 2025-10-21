#pragma once

#include "../error.hpp"
#include "machine.hpp"

#include <memory>

namespace ls_gitea_runner {

class machine_manager {
public:
    virtual ~machine_manager() = default;
    virtual std::expected<std::unique_ptr<machine>, generic_error> spawn(const std::string& options) = 0;
};

} // namespace ls_gitea_runner
