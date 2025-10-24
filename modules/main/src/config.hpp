#pragma once

#include "error.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <map>
#include <vector>

namespace ls_gitea_runner::config {

struct docker_config {
    std::string image;
    std::string tag;
};

struct qemu_config {
    std::string image;
    std::string cpu;
    std::size_t memory;
};

struct runner_environment_config {
    std::vector<std::string> labels;
    std::string os;
    std::string arch;
    std::string temp_dir;
    std::string workspaces_dir;
    std::variant<docker_config, qemu_config> details;
    std::string details_as_json;
};

struct runner_config {
    std::string instance_url;
    std::string name;
    std::string token;
    bool ephemeral{};
    std::map<std::string, runner_environment_config> environments;

    std::optional<std::tuple<std::string, runner_environment_config>>
    find_environment_by_label(const std::string_view search_label) const noexcept;
};

std::expected<runner_config, generic_error> load_file(const std::filesystem::path& file_path) noexcept;

} // namespace ls_gitea_runner::config
