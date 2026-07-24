export module utility.fs:util;

import utility.misc;

import std;
import std.compat;

namespace ls_gitea_runner::fs {

void read_file_into_internal(std::span<std::byte> content, const std::filesystem::path& file_path) {
    std::ifstream stream{file_path, std::ios_base::binary};
    stream.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    stream.read(reinterpret_cast<char*>(content.data()), utility::safe_cast_int<std::streamsize>(content.size_bytes()));
}

export std::filesystem::path temporary_file_path(const std::optional<std::string>& prefix = std::nullopt,
                                                 const std::optional<std::filesystem::path>& base_dir = std::nullopt) {
    static constexpr std::array<char, 36> alphabet = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
                                                      'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                                      'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
    static constexpr int max_attempts{3};
    static constexpr int min_random_length{6};
    static constexpr int max_random_length{32};
    const auto& prefix_{prefix.value_or("tmp.")};
    const auto& temp_dir{base_dir.value_or(std::filesystem::temp_directory_path())};
    // NOLINTNEXTLINE(misc-use-internal-linkage): False positive
    thread_local std::mt19937 rng{std::random_device{}()};
    // NOLINTNEXTLINE(misc-use-internal-linkage): False positive
    thread_local std::uniform_int_distribution<uint8_t> dist{0, alphabet.size() - 1};
    for (int attempt{}; attempt < max_attempts; ++attempt) {
        auto file_name{prefix_};
        for (int i{}; i < min_random_length; ++i) {
            file_name += alphabet[dist(rng)]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        for (int i{}; i < max_random_length - min_random_length; ++i) {
            file_name += alphabet[dist(rng)]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            const auto p{temp_dir / file_name};
            if (!exists(p)) {
                return p;
            }
        }
    }
    throw std::runtime_error{"Failed to generate a non-existing temporary file path"};
}

export template <typename T>
    requires utility::contiguous_byte_container<T>
void read_file_into(T& content, const std::filesystem::path& file_path) {
    const auto file_size{std::filesystem::file_size(file_path)};
    content.resize(file_size);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    read_file_into_internal(std::span<std::byte>{reinterpret_cast<std::byte*>(content.data()), content.size()},
                            file_path);
}

export template <typename T = std::vector<std::byte>>
    requires utility::contiguous_byte_container<T>
T read_file(const std::filesystem::path& file_path) {
    T content;
    read_file_into(content, file_path);
    return content;
}

export void write_file(const std::filesystem::path& file_path, std::span<const std::byte> content) {
    std::ofstream stream{file_path, std::ios_base::binary};
    stream.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    stream.write(reinterpret_cast<const char*>(content.data()),
                 utility::safe_cast_int<std::streamsize>(content.size()));
}

} // namespace ls_gitea_runner::fs
