#pragma once

#include "../error.hpp"
#include "../machine/machine_pool_details.hpp"

#include <expected>
#include <string>

namespace ls_gitea_runner {

struct DockerMachineOptions {
    std::string image;

    static std::expected<DockerMachineOptions, GenericError> load(const MachinePoolDetails& details);
};

} // namespace ls_gitea_runner
