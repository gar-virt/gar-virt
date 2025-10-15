#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace ls_gitea_runner::fs {

std::filesystem::path temporary_file_path(std::optional<std::string> prefix = std::nullopt,
                                          std::optional<std::filesystem::path> base_dir = std::nullopt);

} // namespace ls_gitea_runner::fs
