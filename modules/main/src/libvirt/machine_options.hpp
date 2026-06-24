#pragma once

#include "utility/error.hpp"
#include "../machine/machine_pool_details.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace ls_gitea_runner {

struct LibvirtMachineOptions {
    std::string hypervisor_uri;
    std::filesystem::path domain_template_path;
    std::filesystem::path volume_template_path;
    std::string storage_pool_name;

    static std::expected<LibvirtMachineOptions, GenericError> load(const MachinePoolDetails& details);
};

} // namespace ls_gitea_runner
