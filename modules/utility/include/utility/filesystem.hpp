#pragma once

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ls_gitea_runner::fs {
namespace detail {

template <typename T>
concept contiguous_byte_container =
    requires {
        typename T::iterator;
        typename T::value_type;
    } && std::contiguous_iterator<typename T::iterator> &&
    (std::same_as<typename T::value_type, std::byte> ||
     (std::integral<typename T::value_type> && (sizeof(typename T::value_type) == 1)));

void read_file_into(std::span<std::byte> content, const std::filesystem::path& file_path);

} // namespace detail

std::filesystem::path temporary_file_path(std::optional<std::string> prefix = std::nullopt,
                                          std::optional<std::filesystem::path> base_dir = std::nullopt);

template <typename T>
    requires detail::contiguous_byte_container<T>
void read_file_into(T& content, const std::filesystem::path& file_path) {
    const auto file_size{std::filesystem::file_size(file_path)};
    content.resize(file_size);
    detail::read_file_into(std::span<std::byte>{reinterpret_cast<std::byte*>(content.data()), content.size()},
                           file_path);
}

template <typename T = std::vector<std::byte>>
    requires detail::contiguous_byte_container<T>
T read_file(const std::filesystem::path& file_path) {
    T content;
    read_file_into(content, file_path);
    return content;
}

void write_file(const std::filesystem::path& file_path, std::span<const std::byte> content);

} // namespace ls_gitea_runner::fs
