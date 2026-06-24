#pragma once

#include <filesystem>
#include <string>

namespace ls_gitea_runner {

struct MachinePoolDetails {
    std::filesystem::path config_dir;
    std::string details_as_yaml;
};

} // namespace ls_gitea_runner
