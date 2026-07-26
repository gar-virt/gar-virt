#include "backend.hpp"

#include "factory.hpp"
#include "libvirt.hpp"

#include <utility/fs/filesystem.hpp>
#include <utility/result.hpp>
#include <utility/string.hpp>
#include <utility/uuid.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <thread>

#include <yaml-cpp/yaml.h>

namespace gv::virt {
namespace {
Result<std::vector<std::string>> add_command_output_redirection(const std::string& target_os,
                                                                const std::vector<std::string>& args) {
    std::vector<std::string> cmd;
    if (utility::string_compare_ci(target_os, "linux") == 0 || utility::string_compare_ci(target_os, "macos") == 0) {
        cmd.emplace_back("sh");
        cmd.emplace_back("-c");
        cmd.emplace_back("exec \"$@\" >2&1");
        for (const auto& arg : args) {
            cmd.emplace_back(arg);
        }
    } else if (utility::string_compare_ci(target_os, "windows") == 0) {
        cmd.emplace_back("powershell");
        cmd.emplace_back("-NoProfile");
        cmd.emplace_back("-NonInteractive");
        cmd.emplace_back("-Command");
        cmd.emplace_back("& @args 2>&1");
        for (const auto& arg : args) {
            cmd.emplace_back(arg);
        }
    } else {
        return std::unexpected{
            Error{std::format("Output redirection command not implemented for target OS {}", target_os)}};
    }
    return cmd;
}

} // namespace

Result<LibvirtMachineTemplateDetails> LibvirtMachineTemplateDetails::load(const std::string& details,
                                                                          const std::filesystem::path& config_dir) {
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

Result<LibvirtMachinePoolDetails> LibvirtMachinePoolDetails::load(const std::string& details) {
    try {
        const auto y{YAML::Load(details)};
        return LibvirtMachinePoolDetails{
            .hypervisor_uri = y["hypervisor_uri"].as<std::string>(),
        };
    } catch (const std::exception& ex) {
        return std::unexpected{std::format("Unable to parse Libvirt machine pool details: {}", ex.what())};
    }
}

class LibvirtMachine::Impl final {
public:
    Impl(libvirt::Hypervisor hv, std::shared_ptr<libvirt::Machine> underlying_machine, Info info)
            : m_hv{std::move(hv)}, m_underlying_machine{std::move(underlying_machine)},
              m_id{m_underlying_machine->get_name()}, m_info{std::move(info)} {}

    ~Impl() { std::ignore = terminate(); }

    Impl(const Impl&) = delete;
    Impl(Impl&&) = default;

    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = default;

    const std::string& get_id() const { return m_id; }

    Result<void> terminate() { return m_underlying_machine->kill(); }

    Result<SpawnResult> shell_exec(const std::vector<std::string>& cmd,
                                   const std::optional<std::chrono::seconds>& timeout) const {

        const auto fixed_cmd{add_command_output_redirection(m_info.os, cmd)};
        return m_underlying_machine->shell_exec(cmd, timeout).transform([](auto res) {
            return SpawnResult{.exit_code = res.exit_code, .output = std::move(res.output)};
        });
    }

    Result<void> wait_for_guest_agent(std::chrono::milliseconds timeout, const utility::ShutdownSignal& stop) {
        using namespace std::literals;
        const auto start_time{std::chrono::steady_clock::now()};
        while (true) {
            if (stop.is_signalled()) {
                return std::unexpected{Error{std::format("Shutting down")}};
            }
            auto ready_res{m_underlying_machine->is_ready()};
            if (!ready_res) {
                return std::unexpected{ready_res.error()};
            }
            if (*ready_res) {
                return {};
            }
            if (std::chrono::steady_clock::now() - start_time >= timeout) {
                break;
            }
            std::this_thread::sleep_for(200ms);
        }
        return std::unexpected{Error{std::format("Timed out while waiting for machine {} guest agent.", get_id())}};
    }

    Result<void> write_file(const std::string& remote_path, std::span<const std::byte> content) {
        return m_underlying_machine->write_file(remote_path, content);
    }

    const Machine::Info& info() const { return m_info; }

private:
    libvirt::Hypervisor m_hv;
    std::shared_ptr<libvirt::Machine> m_underlying_machine;
    std::string m_id;
    Machine::Info m_info;
};

LibvirtMachine::LibvirtMachine(std::unique_ptr<Impl> impl) : m_impl{std::move(impl)} {}

LibvirtMachine::~LibvirtMachine() = default;

const std::string& LibvirtMachine::get_id() const { return m_impl->get_id(); }

Result<void> LibvirtMachine::terminate() { return m_impl->terminate(); }

Result<SpawnResult> LibvirtMachine::shell_exec(const std::vector<std::string>& cmd,
                                               const std::optional<std::chrono::seconds>& timeout) const {
    return m_impl->shell_exec(cmd, timeout);
}

Result<void> LibvirtMachine::wait_for_guest_agent(std::chrono::seconds timeout, const utility::ShutdownSignal& stop) {
    return m_impl->wait_for_guest_agent(timeout, stop);
}

Result<void> LibvirtMachine::write_file_impl(const std::string& remote_path, std::span<const std::byte> content) {
    return m_impl->write_file(remote_path, content);
}

const Machine::Info& LibvirtMachine::info() const { return m_impl->info(); }

class LibvirtBackend::Impl final {
public:
    Result<std::unique_ptr<Machine>> spawn(const Machine::Info& info, const std::string& serialized_pool_details,
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

        return std::make_unique<LibvirtMachine>(
            std::make_unique<LibvirtMachine::Impl>(*std::move(hv), *std::move(spawn_res), info));
    }
};

LibvirtBackend::LibvirtBackend() : m_impl{std::make_unique<Impl>()} {}

LibvirtBackend::~LibvirtBackend() = default;

Result<std::unique_ptr<Machine>> LibvirtBackend::spawn(const Machine::Info& info,
                                                       const std::string& serialized_pool_details,
                                                       const std::string& serialized_template_details,
                                                       const std::filesystem::path& config_dir) {
    return m_impl->spawn(info, serialized_pool_details, serialized_template_details, config_dir);
}

Result<std::unique_ptr<Backend>> create_libvirt_backend() { return std::make_unique<LibvirtBackend>(); }

} // namespace gv::virt
