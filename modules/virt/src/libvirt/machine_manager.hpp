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

    std::expected<std::unique_ptr<Machine>, GenericError> spawn(Machine::Info info,
                                                                MachinePoolDetails details) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner
