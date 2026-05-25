#pragma once

#include "error.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace ls_gitea_runner {

struct RuntimeState {
    std::string uuid;
    std::string token;

    std::expected<void, GenericError> save();

    static std::expected<RuntimeState, GenericError> load_file(const std::filesystem::path& file_path);

    static std::expected<RuntimeState, GenericError> create(const std::filesystem::path& file_path);

private:
    RuntimeState(const std::filesystem::path& file_path);

    std::filesystem::path m_file_path;
};

} // namespace ls_gitea_runner
