#pragma once

#include "libvirt.hpp"

#include <virt/machine.hpp>

#include <memory>

namespace gv {

class LibvirtMachine final : public Machine {
public:
    LibvirtMachine(libvirt::Hypervisor hv, std::shared_ptr<libvirt::Machine> underlying_machine, Info info);
    ~LibvirtMachine();

    const std::string& get_id() const override;
    std::expected<void, Error> terminate() override;
    std::expected<SpawnResult, Error>
    shell_exec(const std::vector<std::string>& cmd, const std::optional<std::chrono::seconds>& timeout) const override;
    std::expected<void, Error> wait_for_guest_agent(std::chrono::seconds timeout,
                                                           const utility::ShutdownSignal& stop) override;
    const Info& info() const override;

private:
    std::expected<void, Error> write_file_impl(const std::string& remote_path,
                                                      std::span<const std::byte>) override;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace gv
