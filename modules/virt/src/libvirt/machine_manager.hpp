#pragma once

#include <virt/machine.hpp>
#include <virt/machine_manager.hpp>

#include <utility/error.hpp>

#include <expected>
#include <memory>

namespace ls_gitea_runner {

class LibvirtMachineManager final : public MachineManager {
public:
    LibvirtMachineManager();
    ~LibvirtMachineManager();

    std::expected<std::unique_ptr<Machine>, GenericError> spawn(const Machine::Info& info,
                                                                const std::string& serialized_pool_details,
                                                                const std::string& serialized_template_details,
                                                                const std::filesystem::path& config_dir) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner
