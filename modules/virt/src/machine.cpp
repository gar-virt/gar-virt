#include <virt/machine.hpp>

#include <utility/string.hpp>

#include <filesystem>
#include <string>

namespace ls_gitea_runner {

std::string Machine::make_temp_path(const std::string& sub_path) const {
    const auto delimiter{utility::string_compare_ci(info().os, "windows") == 0 ? "\\" : "/"};
    auto result{utility::string_replace(
        utility::string_from_u8string(
            (std::filesystem::path{info().temp_dir} / utility::u8string_from_string(sub_path)).generic_u8string()),
        "/", delimiter)};
    return result;
}

} // namespace ls_gitea_runner
