#include "machine_template_details.hpp"

#include <yaml-cpp/yaml.h>

#include <format>

namespace ls_gitea_runner {

std::expected<LibvirtMachineTemplateDetails, GenericError>
LibvirtMachineTemplateDetails::load(const std::string& details, const std::filesystem::path& config_dir) {
    try {
        const auto y{YAML::Load(details)};
        LibvirtMachineTemplateDetails details{
            .domain_template_path = y["domain_template_path"].as<std::string>(),
            .volume_template_path = y["volume_template_path"].as<std::string>(),
            .storage_pool_name = y["storage_pool_name"].as<std::string>(),
        };
        if (details.domain_template_path.is_relative()) {
            details.domain_template_path = config_dir / details.domain_template_path;
        }
        if (details.volume_template_path.is_relative()) {
            details.volume_template_path = config_dir / details.volume_template_path;
        }
        return details;
    } catch (const std::exception& ex) {
        return std::unexpected{std::format("Unable to parse Libvirt machine template details: {}", ex.what())};
    }
}

} // namespace ls_gitea_runner
