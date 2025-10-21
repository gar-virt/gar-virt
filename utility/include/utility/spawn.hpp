#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ls_gitea_runner::utility {

struct spawn_result {
    int exit_code{};
    std::string output;
};

struct spawn_options {
    std::function<std::int64_t(char*, int)> stdin_writer;
    std::function<std::int64_t(const char*, int)> stdout_reader;
};

std::string spawn_escape_arg(const std::string_view arg);
std::expected<int, std::runtime_error> spawn_cmd(const std::vector<std::string>& cmd, spawn_options options);
std::expected<spawn_result, std::runtime_error> spawn_cmd(const std::string cmd);
std::expected<spawn_result, std::runtime_error> spawn_cmd(const std::vector<std::string>& cmd);

} // namespace ls_gitea_runner::utility
