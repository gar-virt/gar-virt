#pragma once

#include <utility/result.hpp>

#include <expected>
#include <string>

namespace gv::virt {

struct LibvirtMachinePoolDetails {
    std::string hypervisor_uri;

    static Result<LibvirtMachinePoolDetails> load(const std::string& details);
};

} // namespace gv::virt
