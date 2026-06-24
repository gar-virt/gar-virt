#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ls_gitea_runner::fs {

std::filesystem::path temporary_file_path(std::optional<std::string> prefix = std::nullopt,
                                          std::optional<std::filesystem::path> base_dir = std::nullopt);

std::vector<std::byte> read_file(const std::filesystem::path& file_path);
void write_file(const std::filesystem::path& file_path, std::span<const std::byte> content);

} // namespace ls_gitea_runner::fs
