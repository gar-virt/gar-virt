#pragma once

#include <utility/error.hpp>
#include <utility/log/log.hpp>
#include <virt/arch.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace ls_gitea_runner::config {

struct MachineTemplateConfig {
    std::string os;
    Arch::Type arch{};
    std::string temp_dir;
    size_t idle_target{};
    size_t max_concurrency{};
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

struct LogConfig {
    utility::LogLevel level;
};

struct BackendConfig {
    std::string type;
    std::string name;
    std::vector<MachineTemplateConfig> templates;
    std::string raw_details;
};

struct MainConfig {
    std::filesystem::path base_dir;
    size_t config_version{};
    LogConfig log;
    std::string name;
    ForgeConfig forge;
    std::vector<BackendConfig> backends;

    void resolve(const std::filesystem::path& base_dir);
};

std::expected<MainConfig, GenericError> load_file(const std::filesystem::path& file_path) noexcept;

} // namespace ls_gitea_runner::config
