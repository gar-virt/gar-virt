#include "docker_machine.hpp"
#include "docker_engine_client.hpp"

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

class docker_machine::impl final {
public:
    impl(const docker_container_id& id, info_t info) : m_id{id}, m_info{std::move(info)} {}

    ~impl() { std::ignore = terminate(); }

    const std::string& get_id() const { return m_id.value; }

    std::expected<void, generic_error> terminate() { return m_docker.container_kill(m_id); }

    std::expected<int, generic_error> shell_exec(const std::vector<std::string>& cmd,
                                                 utility::spawn_options options) const {
        return m_docker.container_exec(m_id, cmd, std::move(options));
    }

    std::expected<utility::spawn_result, generic_error> shell_exec(const std::vector<std::string>& cmd) const {
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

    std::expected<void, generic_error> copy_file_into(const std::filesystem::path& local_path,
                                                      const std::string& remote_path) {
        return m_docker.container_cp_into(m_id, local_path, remote_path);
    }

    const machine::info_t& info() const { return m_info; }

private:
    docker_container_id m_id;
    machine::info_t m_info;
    docker_engine_client m_docker;
};

docker_machine::docker_machine(const docker_container_id& id, info_t info)
        : m_impl{std::make_unique<impl>(id, std::move(info))} {}

docker_machine::~docker_machine() {}

const std::string& docker_machine::get_id() const { return m_impl->get_id(); }

std::expected<void, generic_error> docker_machine::terminate() { return m_impl->terminate(); }

std::expected<int, generic_error> docker_machine::shell_exec(const std::vector<std::string>& cmd,
                                                             utility::spawn_options options) const {
    return m_impl->shell_exec(cmd, std::move(options));
}

std::expected<utility::spawn_result, generic_error>
docker_machine::shell_exec(const std::vector<std::string>& cmd) const {
    return m_impl->shell_exec(cmd);
}

bool docker_machine::wait_until_ready(std::chrono::seconds timeout) { return m_impl->wait_until_ready(timeout); }

std::expected<void, generic_error> docker_machine::copy_file_into(const std::filesystem::path& local_path,
                                                                  const std::string& remote_path) {
    return m_impl->copy_file_into(local_path, remote_path);
}

const machine::info_t& docker_machine::info() const { return m_impl->info(); }

} // namespace ls_gitea_runner
