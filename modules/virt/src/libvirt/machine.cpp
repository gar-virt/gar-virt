#include "machine.hpp"

#include <utility/string.hpp>

#include <chrono>
#include <thread>
#include <tuple>

namespace ls_gitea_runner {
namespace {
std::expected<std::vector<std::string>, GenericError>
add_command_output_redirection(const std::string& target_os, const std::vector<std::string>& args) {
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
            GenericError{std::format("Output redirection command not implemented for target OS {}", target_os)}};
    }
    return cmd;
}
} // namespace

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

    std::expected<void, GenericError> terminate() { return m_underlying_machine->kill(); }

    std::expected<SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                        const std::optional<std::chrono::seconds>& timeout) const {

        const auto fixed_cmd{add_command_output_redirection(m_info.os, cmd)};
        return m_underlying_machine->shell_exec(cmd, timeout).transform([](auto res) {
            return SpawnResult{.exit_code = res.exit_code, .output = std::move(res.output)};
        });
    }

    std::expected<void, GenericError> wait_for_guest_agent(std::chrono::milliseconds timeout,
                                                           utility::ShutdownSignal stop) {
        using namespace std::literals;
        const auto start_time{std::chrono::steady_clock::now()};
        while (true) {
            if (stop.is_signalled()) {
                return std::unexpected{GenericError{std::format("Shutting down")}};
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
        return std::unexpected{
            GenericError{std::format("Timed out while waiting for machine {} guest agent.", get_id())}};
    }

    std::expected<void, GenericError> write_file(const std::string& remote_path, std::span<const std::byte> content) {
        return m_underlying_machine->write_file(remote_path, content);
    }

    const Machine::Info& info() const { return m_info; }

private:
    libvirt::Hypervisor m_hv;
    std::shared_ptr<libvirt::Machine> m_underlying_machine;
    std::string m_id;
    Machine::Info m_info;
};

LibvirtMachine::LibvirtMachine(libvirt::Hypervisor hv, std::shared_ptr<libvirt::Machine> underlying_machine, Info info)
        : m_impl{std::make_unique<Impl>(std::move(hv), std::move(underlying_machine), std::move(info))} {}

LibvirtMachine::~LibvirtMachine() {}

const std::string& LibvirtMachine::get_id() const { return m_impl->get_id(); }

std::expected<void, GenericError> LibvirtMachine::terminate() { return m_impl->terminate(); }

std::expected<SpawnResult, GenericError>
LibvirtMachine::shell_exec(const std::vector<std::string>& cmd,
                           const std::optional<std::chrono::seconds>& timeout) const {
    return m_impl->shell_exec(cmd, timeout);
}

std::expected<void, GenericError> LibvirtMachine::wait_for_guest_agent(std::chrono::seconds timeout,
                                                                       utility::ShutdownSignal stop) {
    return m_impl->wait_for_guest_agent(timeout, stop);
}

std::expected<void, GenericError> LibvirtMachine::write_file_impl(const std::string& remote_path,
                                                                  std::span<const std::byte> content) {
    return m_impl->write_file(remote_path, content);
}

const Machine::Info& LibvirtMachine::info() const { return m_impl->info(); }

} // namespace ls_gitea_runner
