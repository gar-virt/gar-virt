#pragma once

#include <utility/error.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace ls_gitea_runner::config {

struct MachinePoolConfig {
    std::string provider;
    size_t capacity{};
    std::string os;
    std::string arch;
    std::string runner_exe_path;
    std::string details_as_yaml;
};

struct ForgeTokenConfig {
    std::string source; // env, file, inline?
    std::string value;

    std::string resolve(const std::filesystem::path& base_dir);
};

struct ForgeConfig {
    std::string type;
    std::string uri;
    ForgeTokenConfig token_config;
    std::string token;
};

struct RunnerConfig {
    std::filesystem::path config_base_dir;
    size_t config_version{};
    std::string name;
    ForgeConfig forge;
    std::vector<std::string> labels;
    MachinePoolConfig machine_pool;

    std::vector<std::string> get_label_names() const;
};

std::expected<RunnerConfig, GenericError> load_file(const std::filesystem::path& file_path) noexcept;

} // namespace ls_gitea_runner::config
