#pragma once

#include "error.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <unordered_map>
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
    std::variant<docker_config, qemu_config> details;
};

struct runner_config {
    std::string instance_url;
    std::string name;
    std::string token;
    bool ephemeral{};
    std::unordered_map<std::string, runner_environment_config> environments;
};

std::expected<runner_config, generic_error> load_file(const std::filesystem::path& file_path) noexcept;

} // namespace ls_gitea_runner::config
