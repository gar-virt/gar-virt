#pragma once

#include "libvirt.hpp"

#include <virt/machine.hpp>

#include <memory>

namespace ls_gitea_runner {

class LibvirtMachine final : public Machine {
public:
    LibvirtMachine(libvirt::Hypervisor hv, std::shared_ptr<libvirt::Machine> underlying_machine, Info info);
    ~LibvirtMachine();

    const std::string& get_id() const override;
    std::expected<void, GenericError> terminate() override;
    std::expected<int, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                utility::SpawnOptions options) const override;
    std::expected<utility::SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd) const override;
    std::expected<void, GenericError> wait_for_guest_agent(std::chrono::seconds timeout,
                                                           utility::ShutdownSignal stop) override;
    std::expected<void, GenericError> copy_file_into(const std::filesystem::path& local_path,
                                                     const std::string& remote_path) override;
    std::expected<void, GenericError> write_file(const std::string& remote_path, std::span<const std::byte>) override;
    const Info& info() const override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner
