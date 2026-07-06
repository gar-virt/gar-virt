#include "machine_pool_details.hpp"

#include <yaml-cpp/yaml.h>

#include <format>

namespace ls_gitea_runner {

std::expected<LibvirtMachinePoolDetails, GenericError>
LibvirtMachinePoolDetails::load(const std::string& details) {
    try {
        const auto y{YAML::Load(details)};
        return LibvirtMachinePoolDetails{
            .hypervisor_uri = y["hypervisor_uri"].as<std::string>(),
        };
    } catch (const std::exception& ex) {
        return std::unexpected{std::format("Unable to parse Libvirt machine pool details: {}", ex.what())};
    }
}

} // namespace ls_gitea_runner
