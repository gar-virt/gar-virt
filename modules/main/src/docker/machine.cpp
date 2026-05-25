#include "machine.hpp"
#include "engine_client.hpp"

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

class DockerMachine::Impl final {
public:
    Impl(const DockerContainerId& id, Info info) : m_id{id}, m_info{std::move(info)} {}

    ~Impl() { std::ignore = terminate(); }

    const std::string& get_id() const { return m_id.value; }

    std::expected<void, GenericError> terminate() { return m_docker.container_kill(m_id); }

    std::expected<int, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                utility::SpawnOptions options) const {
        return m_docker.container_exec(m_id, cmd, std::move(options));
    }

    std::expected<utility::SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd) const {
        return m_docker.container_exec(m_id, cmd);
    }

    bool wait_until_ready(std::chrono::seconds timeout) {
        using namespace std::literals;
        const auto start_time{std::chrono::steady_clock::now()};
        while (auto res{m_docker.container_is_running(m_id)}) {
            if (auto running{*res}) {
                return running;
            }
            if (std::chrono::steady_clock::now() - start_time >= timeout) {
                break;
            }
            std::this_thread::sleep_for(200ms);
        }
        return false;
    }

    std::expected<void, GenericError> copy_file_into(const std::filesystem::path& local_path,
                                                     const std::string& remote_path) {
        return m_docker.container_cp_into(m_id, local_path, remote_path);
    }

    const Machine::Info& info() const { return m_info; }

private:
    DockerContainerId m_id;
    Machine::Info m_info;
    DockerEngineClient m_docker;
};

DockerMachine::DockerMachine(const DockerContainerId& id, Info info)
        : m_impl{std::make_unique<Impl>(id, std::move(info))} {}

DockerMachine::~DockerMachine() {}

const std::string& DockerMachine::get_id() const { return m_impl->get_id(); }

std::expected<void, GenericError> DockerMachine::terminate() { return m_impl->terminate(); }

std::expected<int, GenericError> DockerMachine::shell_exec(const std::vector<std::string>& cmd,
                                                           utility::SpawnOptions options) const {
    return m_impl->shell_exec(cmd, std::move(options));
}

std::expected<utility::SpawnResult, GenericError> DockerMachine::shell_exec(const std::vector<std::string>& cmd) const {
    return m_impl->shell_exec(cmd);
}

bool DockerMachine::wait_until_ready(std::chrono::seconds timeout) { return m_impl->wait_until_ready(timeout); }

std::expected<void, GenericError> DockerMachine::copy_file_into(const std::filesystem::path& local_path,
                                                                const std::string& remote_path) {
    return m_impl->copy_file_into(local_path, remote_path);
}

const Machine::Info& DockerMachine::info() const { return m_impl->info(); }

} // namespace ls_gitea_runner
