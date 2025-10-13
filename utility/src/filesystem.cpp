#include <utility/filesystem.hpp>

#include <filesystem>
#include <optional>
#include <random>
#include <string>

namespace ls_gitea_runner::fs {

std::filesystem::path temporary_file_path(std::optional<std::string> prefix) {
    static constexpr char alphabet[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
                                        'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                        'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
    const auto prefix_{prefix.value_or("tmp.")};
    const auto temp_dir{std::filesystem::temp_directory_path()};
    std::random_device dev;
    std::mt19937 rng{dev()};
    std::uniform_int_distribution<uint8_t> dist{0, (sizeof(alphabet) / sizeof(alphabet[0])) - 1};
    for (int i{}; i < 3; ++i) {
        auto file_name{prefix_};
        for (int j{}; j < 6; ++j) {
            file_name += alphabet[dist(rng)];
        }
        for (int j{}; j < 32 - 6; ++j) {
            file_name += alphabet[dist(rng)];
            const auto p{temp_dir / file_name};
            if (!exists(p)) {
                return p;
            }
        }
    }
    std::abort();
}

} // namespace ls_gitea_runner::fs
