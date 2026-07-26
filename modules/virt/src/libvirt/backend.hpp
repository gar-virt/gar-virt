#pragma once

#include <virt/api.hpp>

#include <utility/result.hpp>

#include <memory>

namespace gv::virt {

struct LibvirtMachineTemplateDetails {
    std::filesystem::path domain_template_path;
    std::filesystem::path volume_template_path;
    std::string storage_pool_name;

    static Result<LibvirtMachineTemplateDetails> load(const std::string& details,
                                                      const std::filesystem::path& config_dir);
};

struct LibvirtMachinePoolDetails {
    std::string hypervisor_uri;

    static Result<LibvirtMachinePoolDetails> load(const std::string& details);
};

class LibvirtMachine final : public Machine {
public:
    class Impl;

    LibvirtMachine(std::unique_ptr<Impl> impl);
    ~LibvirtMachine();

    const std::string& get_id() const override;
    Result<void> terminate() override;
    Result<SpawnResult> shell_exec(const std::vector<std::string>& cmd,
                                   const std::optional<std::chrono::seconds>& timeout) const override;
    Result<void> wait_for_guest_agent(std::chrono::seconds timeout, const utility::ShutdownSignal& stop) override;
    const Info& info() const override;

private:
    Result<void> write_file_impl(const std::string& remote_path, std::span<const std::byte>) override;

    std::unique_ptr<Impl> m_impl;
};

class LibvirtBackend final : public Backend {
public:
    LibvirtBackend();
    ~LibvirtBackend();

    Result<std::unique_ptr<Machine>> spawn(const Machine::Info& info, const std::string& serialized_pool_details,
                                           const std::string& serialized_template_details,
                                           const std::filesystem::path& config_dir) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace gv::virt
