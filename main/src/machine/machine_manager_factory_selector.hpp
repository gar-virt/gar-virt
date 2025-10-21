#pragma once

#include "../error.hpp"
#include "machine_manager_factory.hpp"

#include <memory>

namespace ls_gitea_runner {

class machine_manager_factory_selector final {
public:
    static std::expected<std::unique_ptr<machine_manager_factory>, generic_error> get_factory(const std::string& name);
};

} // namespace ls_gitea_runner
