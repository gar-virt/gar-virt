module;

#include <filesystem>

export module main;

export namespace ls_gitea_runner {

struct ProgramOptions {
    std::filesystem::path config_file;
};

} // namespace ls_gitea_runner
