#pragma once

#include "machine_manager_factory.hpp"

namespace ls_gitea_runner {

class docker_machine_manager_factory final : public machine_manager_factory {
public:
    ~docker_machine_manager_factory();
    std::unique_ptr<machine_manager> create() override;
};

} // namespace ls_gitea_runner
