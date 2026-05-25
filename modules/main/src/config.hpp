#pragma once

#include "error.hpp"

#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace ls_gitea_runner::config {

struct DockerConfig {
    std::string image;
    std::string tag;
};

struct QemuConfig {
    std::string image;
    std::string cpu;
    std::size_t memory;
};

struct RunnerEnvironmentConfig {
    std::vector<std::string> labels;
    std::string os;
    std::string arch;
    std::string temp_dir;
    std::string workspaces_dir;
    std::variant<DockerConfig, QemuConfig> details;
    std::string details_as_json;
};

struct RunnerConfig {
    std::string instance_url;
    std::string name;
    std::string token;
    bool ephemeral{};
    std::map<std::string, RunnerEnvironmentConfig> environments;

    std::optional<std::tuple<std::string, RunnerEnvironmentConfig>>
    find_environment_by_label(const std::string_view search_label) const noexcept;
};

std::expected<RunnerConfig, GenericError> load_file(const std::filesystem::path& file_path) noexcept;

} // namespace ls_gitea_runner::config
