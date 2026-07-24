module;

#include <filesystem>

export module main:program_options;

namespace ls_gitea_runner {

export struct ProgramOptions {
    std::filesystem::path config_file;
};

} // namespace ls_gitea_runner
