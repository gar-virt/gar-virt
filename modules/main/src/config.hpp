#pragma once

#include <utility/error.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace ls_gitea_runner::config {

struct MachineTemplateConfig {
    std::string os;
    std::string arch;
    std::string runner_exe_path;
    std::vector<std::string> labels;
    std::string raw_details;

    std::vector<std::string> get_label_names() const;
};

struct ForgeTokenConfig {
    std::string source; // env, file, inline?
    std::string value;
    std::string resolved_token;

    void resolve(const std::filesystem::path& base_dir);
};

struct ForgeConfig {
    std::string type;
    std::string uri;
    ForgeTokenConfig token;

    void resolve(const std::filesystem::path& base_dir);
};

struct BackendConfig {
    std::string type;
    size_t capacity{};
    std::vector<MachineTemplateConfig> templates;
    std::string raw_details;
};

struct MainConfig {
    std::filesystem::path base_dir;
    size_t config_version{};
    std::string name;
    ForgeConfig forge;
    std::vector<BackendConfig> backends;

    void resolve(const std::filesystem::path& base_dir);
};

std::expected<MainConfig, GenericError> load_file(const std::filesystem::path& file_path) noexcept;

} // namespace ls_gitea_runner::config
