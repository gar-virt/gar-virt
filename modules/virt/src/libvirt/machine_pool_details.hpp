#pragma once

#include <utility/error.hpp>

#include <expected>
#include <string>

namespace gv {

struct LibvirtMachinePoolDetails {
    std::string hypervisor_uri;

    static std::expected<LibvirtMachinePoolDetails, Error> load(const std::string& details);
};

} // namespace gv
