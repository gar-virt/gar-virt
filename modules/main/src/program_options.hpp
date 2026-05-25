#pragma once

#include <filesystem>

namespace ls_gitea_runner {

struct ProgramOptions {
    std::filesystem::path config_file;
    std::filesystem::path state_file;
};

} // namespace ls_gitea_runner
