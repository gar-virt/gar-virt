#pragma once

#include "error.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace ls_gitea_runner {

struct runtime_state {
    std::string uuid;
    std::string token;

    std::expected<void, generic_error> save();

    static std::expected<runtime_state, generic_error> load_file(const std::filesystem::path& file_path);

    static std::expected<runtime_state, generic_error> create(const std::filesystem::path& file_path);

private:
    runtime_state(const std::filesystem::path& file_path);

    std::filesystem::path m_file_path;
};

} // namespace ls_gitea_runner
