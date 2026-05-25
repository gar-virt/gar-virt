#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ls_gitea_runner::utility {

struct SpawnResult {
    int exit_code{};
    std::string output;
};

struct SpawnOptions {
    std::function<std::int64_t(char*, int)> stdin_writer;
    std::function<std::int64_t(const char*, int)> stdout_reader;
};

std::expected<int, std::runtime_error> spawn_cmd(const std::vector<std::string>& cmd, SpawnOptions options);
std::expected<SpawnResult, std::runtime_error> spawn_cmd(const std::vector<std::string>& cmd);

} // namespace ls_gitea_runner::utility
