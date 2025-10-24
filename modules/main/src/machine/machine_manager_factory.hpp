#pragma once

#include "machine_manager.hpp"

#include <memory>

namespace ls_gitea_runner {

class machine_manager_factory {
public:
    virtual ~machine_manager_factory() = default;
    virtual std::unique_ptr<machine_manager> create() = 0;
};

} // namespace ls_gitea_runner
