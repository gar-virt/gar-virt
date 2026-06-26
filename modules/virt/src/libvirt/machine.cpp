#include "machine.hpp"

#include <utility/string.hpp>

#include <chrono>
#include <thread>
#include <tuple>

namespace ls_gitea_runner {
namespace {
std::string normalize_uname_kernel_name(const std::string& input) {
    if (input == "Darwin") {
        return "macOS";
    }
    if (input == "WindowsNT") {
        return "Windows";
    }
    if (utility::string_starts_with(input, "CYGWIN")) {
        return "Windows";
    }
    if (utility::string_starts_with(input, "MINGW")) {
        return "Windows";
    }
    if (utility::string_starts_with(input, "MSYS")) {
        return "Windows";
    }
    return "Linux";
}

std::string normalize_uname_machine(const std::string& input) {
    // TODO: X86, ARM
    if (input == "x86_64") {
        return "X64";
    }
    if (utility::string_starts_with(input, "aarch64")) {
        return "ARM64";
    }
    return "unknown";
}
} // namespace

class LibvirtMachine::Impl final {
public:
    Impl(libvirt::Hypervisor hv, std::shared_ptr<libvirt::Machine> underlying_machine, Info info)
            : m_hv{std::move(hv)}, m_underlying_machine{std::move(underlying_machine)},
              m_id{m_underlying_machine->get_name()}, m_info{std::move(info)} {}

    ~Impl() { std::ignore = terminate(); }

    const std::string& get_id() const { return m_id; }

    std::expected<void, GenericError> terminate() { return m_underlying_machine->kill(); }

    std::expected<int, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                utility::SpawnOptions options) const {
        return m_underlying_machine->shell_exec(cmd, std::move(options));
    }

    std::expected<utility::SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd) const {
        return m_underlying_machine->shell_exec(cmd);
    }

    std::expected<void, GenericError> wait_for_guest_agent(std::chrono::milliseconds timeout) {
        using namespace std::literals;
        const auto start_time{std::chrono::steady_clock::now()};
        while (true) {
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

    std::expected<void, GenericError> copy_file_into(const std::filesystem::path& local_path,
                                                     const std::string& remote_path) {
        // return m_libvirt.container_cp_into(m_id, local_path, remote_path);
        std::abort();
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

std::expected<int, GenericError> LibvirtMachine::shell_exec(const std::vector<std::string>& cmd,
                                                            utility::SpawnOptions options) const {
    return m_impl->shell_exec(cmd, std::move(options));
}

std::expected<utility::SpawnResult, GenericError>
LibvirtMachine::shell_exec(const std::vector<std::string>& cmd) const {
    return m_impl->shell_exec(cmd);
}

std::expected<void, GenericError> LibvirtMachine::wait_for_guest_agent(std::chrono::seconds timeout) {
    return m_impl->wait_for_guest_agent(timeout);
}

std::expected<void, GenericError> LibvirtMachine::copy_file_into(const std::filesystem::path& local_path,
                                                                 const std::string& remote_path) {
    return m_impl->copy_file_into(local_path, remote_path);
}

std::expected<void, GenericError> LibvirtMachine::write_file(const std::string& remote_path,
                                                             std::span<const std::byte> content) {
    return m_impl->write_file(remote_path, content);
}

const Machine::Info& LibvirtMachine::info() const { return m_impl->info(); }

} // namespace ls_gitea_runner
