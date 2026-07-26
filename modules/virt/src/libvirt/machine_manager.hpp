#pragma once

#include <virt/machine.hpp>
#include <virt/machine_manager.hpp>

#include <utility/result.hpp>

#include <expected>
#include <memory>

namespace gv::virt {

class LibvirtMachineManager final : public MachineManager {
public:
    LibvirtMachineManager();
    ~LibvirtMachineManager();

    Result<std::unique_ptr<Machine>> spawn(const Machine::Info& info, const std::string& serialized_pool_details,
                                           const std::string& serialized_template_details,
                                           const std::filesystem::path& config_dir) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace gv::virt
