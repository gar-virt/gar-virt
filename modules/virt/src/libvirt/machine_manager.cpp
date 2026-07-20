#include "machine_manager.hpp"
#include "libvirt.hpp"
#include "machine.hpp"
#include "machine_pool_details.hpp"
#include "machine_template_details.hpp"

#include <utility/filesystem.hpp>
#include <utility/uuid.hpp>

namespace ls_gitea_runner {

class LibvirtMachineManager::Impl final {
public:
    std::expected<std::unique_ptr<Machine>, GenericError> spawn(const Machine::Info& info,
                                                                const std::string& serialized_pool_details,
                                                                const std::string& serialized_template_details,
                                                                const std::filesystem::path& config_dir) {
        const auto pool_details{LibvirtMachinePoolDetails::load(serialized_pool_details)};
        if (!pool_details) {
            return std::unexpected{pool_details.error()};
        }

        const auto template_details{LibvirtMachineTemplateDetails::load(serialized_template_details, config_dir)};
        if (!template_details) {
            return std::unexpected{template_details.error()};
        }

        auto hv{libvirt::Hypervisor::connect(pool_details->hypervisor_uri)};
        if (!hv) {
            return std::unexpected{hv.error()};
        }

        auto spawn_res{hv->spawn({
            .volume = fs::read_file<std::string>(template_details->volume_template_path),
            .domain = fs::read_file<std::string>(template_details->domain_template_path),
            .storage_pool = template_details->storage_pool_name,
        })};
        if (!spawn_res) {
            return std::unexpected{spawn_res.error()};
        }

        return std::make_unique<LibvirtMachine>(*std::move(hv), *std::move(spawn_res), info);
    }
};

LibvirtMachineManager::LibvirtMachineManager() : m_impl{std::make_unique<Impl>()} {}

LibvirtMachineManager::~LibvirtMachineManager() = default;

std::expected<std::unique_ptr<Machine>, GenericError>
LibvirtMachineManager::spawn(const Machine::Info& info, const std::string& serialized_pool_details,
                             const std::string& serialized_template_details, const std::filesystem::path& config_dir) {
    return m_impl->spawn(info, serialized_pool_details, serialized_template_details, config_dir);
}

} // namespace ls_gitea_runner
