#pragma once

#include "utility/error.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace ls_gitea_runner::config {

struct LibvirtConfig {
    std::string hypervisor_uri;
    std::filesystem::path volume_template_path;
    std::string storage_pool_name;
};

struct MachinePoolConfig {
    std::string provider;
    int64_t capacity{};
    std::string os;
    std::string arch;
    std::string temp_dir;
    std::string workspaces_dir;
    std::string runner_exe_path;
    // std::variant<LibvirtConfig> details;
    std::string details_as_json;
};

struct RunnerConfig {
    std::filesystem::path config_base_dir;
    std::string instance_url;
    std::string token;
    std::string name;
    std::vector<std::string> labels;
    bool ephemeral{};
    MachinePoolConfig machine_pool;

    std::vector<std::string> get_label_names() const;
};

std::expected<RunnerConfig, GenericError> load_file(const std::filesystem::path& file_path) noexcept;

} // namespace ls_gitea_runner::config
