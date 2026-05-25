#pragma once

#include "../error.hpp"

#include <expected>
#include <string>

namespace ls_gitea_runner {

struct DockerMachineOptions {
    std::string image;

    static std::expected<DockerMachineOptions, GenericError> load(const std::string& json_str);
};

} // namespace ls_gitea_runner
