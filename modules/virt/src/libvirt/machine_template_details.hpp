#pragma once

#include <utility/error.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace ls_gitea_runner {

struct LibvirtMachineTemplateDetails {
    std::filesystem::path domain_template_path;
    std::filesystem::path volume_template_path;
    std::string storage_pool_name;

    static std::expected<LibvirtMachineTemplateDetails, GenericError> load(const std::string& details,
                                                                           const std::filesystem::path& config_dir);
};

} // namespace ls_gitea_runner
