#include "machine_manager.hpp"
#include "container_id.hpp"
#include "libvirt.hpp"
#include "machine.hpp"
#include "machine_options.hpp"

#include <utility/filesystem.hpp>
#include <utility/uuid.hpp>

#include <fstream>

namespace ls_gitea_runner {

class LibvirtMachineManager::Impl final {
public:
    std::expected<std::unique_ptr<Machine>, GenericError> spawn(Machine::Info info, MachinePoolDetails details) {
        const auto parsed_options{LibvirtMachineOptions::load(details)};
        if (!parsed_options) {
            return std::unexpected{parsed_options.error()};
        }

        const auto name{std::format("gar-virt-{}", utility::uuid())};

        /*LibvirtContainerId id;
        std::unique_ptr<LibvirtMachine> machine;

        return libvirt.container_create(name, parsed_options->image, {"/usr/bin/sh", "-c", "sleep 10800"})
            .and_then([&](auto res) {
                id = std::move(res);
                machine = std::make_unique<LibvirtMachine>(id, std::move(info));
                return libvirt.container_start(id);
            })
            .transform([&] { return std::move(machine); });*/

        auto hv{libvirt::Hypervisor::connect(parsed_options->hypervisor_uri)};
        if (!hv) {
            return std::unexpected{hv.error()};
        }

        const auto domain_xml{fs::read_file<std::string>(parsed_options->domain_template_path)};
        const auto volume_xml{fs::read_file<std::string>(parsed_options->volume_template_path)};

        auto spawn_res{hv->spawn({
            .volume = volume_xml,
            .domain = domain_xml,
            .storage_pool = parsed_options->storage_pool_name,
        })};
        if (!spawn_res) {
            return std::unexpected{spawn_res.error()};
        }

        return std::make_unique<LibvirtMachine>(*std::move(hv), *std::move(spawn_res), std::move(info));
    }
};

LibvirtMachineManager::LibvirtMachineManager() : m_impl{std::make_unique<Impl>()} {}

LibvirtMachineManager::~LibvirtMachineManager() {}

std::expected<std::unique_ptr<Machine>, GenericError> LibvirtMachineManager::spawn(Machine::Info info,
                                                                                   MachinePoolDetails details) {
    return m_impl->spawn(std::move(info), std::move(details));
}

} // namespace ls_gitea_runner
