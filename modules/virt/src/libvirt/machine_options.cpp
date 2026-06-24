#include "machine_options.hpp"

#include <boost/json.hpp>

#include <format>

namespace ls_gitea_runner {

std::expected<LibvirtMachineOptions, GenericError> LibvirtMachineOptions::load(const MachinePoolDetails& details) {
    try {
        const auto j{boost::json::parse(details.details_as_json)};
        const auto& j_obj{j.as_object()};
        LibvirtMachineOptions options{
            .hypervisor_uri = std::string{j_obj.at("hypervisor_uri").as_string()},
            .domain_template_path = std::string{j_obj.at("domain_template_path").as_string()},
            .volume_template_path = std::string{j_obj.at("volume_template_path").as_string()},
            .storage_pool_name = std::string{j_obj.at("storage_pool_name").as_string()},
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
