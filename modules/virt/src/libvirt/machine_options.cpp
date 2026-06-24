#include "machine_options.hpp"

#include <yaml-cpp/yaml.h>

#include <format>

namespace ls_gitea_runner {

std::expected<LibvirtMachineOptions, GenericError> LibvirtMachineOptions::load(const MachinePoolDetails& details) {
    try {
        const auto y{YAML::Load(details.details_as_yaml)};
        LibvirtMachineOptions options{
            .hypervisor_uri = y["hypervisor_uri"].as<std::string>(),
            .domain_template_path = y["domain_template_path"].as<std::string>(),
            .volume_template_path = y["volume_template_path"].as<std::string>(),
            .storage_pool_name = y["storage_pool_name"].as<std::string>(),
        };
        if (options.domain_template_path.is_relative()) {
            options.domain_template_path = details.config_dir / options.domain_template_path;
        }
        if (options.volume_template_path.is_relative()) {
            options.volume_template_path = details.config_dir / options.volume_template_path;
        }
        return options;
    } catch (const std::exception& ex) {
        return std::unexpected{std::format("Unable to parse Libvirt machine options: {}", ex.what())};
    }
}

} // namespace ls_gitea_runner
