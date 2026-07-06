#pragma once

#include <utility/error.hpp>

#include <expected>
#include <string>

namespace ls_gitea_runner {

struct LibvirtMachinePoolDetails {
    std::string hypervisor_uri;

    static std::expected<LibvirtMachinePoolDetails, GenericError> load(const std::string& details);
};

} // namespace ls_gitea_runner
