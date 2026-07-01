#pragma once

#include "utility/error.hpp"
#include "utility/spawn.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ls_gitea_runner::libvirt {

struct SpawnOptions {
    std::string_view volume;
    std::string_view domain;
    std::string_view storage_pool;
};

class MachineImpl;
class HypervisorImpl;

class Machine final {
public:
    Machine(std::unique_ptr<MachineImpl> impl);
    ~Machine();

    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;

    Machine(Machine&&) noexcept;
    Machine& operator=(Machine&&) noexcept;

    std::string get_name() const noexcept;
    std::expected<void, GenericError> wait() noexcept;
    std::expected<void, GenericError> wait_for_guest_agent();

    std::expected<void, GenericError> write_file(const std::string& file_path,
                                                 std::span<const std::byte> content) noexcept;
    std::expected<utility::SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd) noexcept;
    std::expected<int, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                utility::SpawnOptions options) noexcept;

    std::expected<void, GenericError> resume() noexcept;
    std::expected<void, GenericError> kill() noexcept;
    std::expected<bool, GenericError> is_ready() const noexcept;

private:
    friend HypervisorImpl;
    void notify_bad_state();
    void notify_ready();

    std::unique_ptr<MachineImpl> m_impl;
};

class Hypervisor final {
public:
    Hypervisor(std::unique_ptr<HypervisorImpl> impl);
    ~Hypervisor();

    Hypervisor(const Hypervisor&) = delete;
    Hypervisor& operator=(const Hypervisor&) = delete;

    Hypervisor(Hypervisor&&) noexcept;
    Hypervisor& operator=(Hypervisor&&) noexcept;

    // std::expected<void, GenericError> run() noexcept;
    // void stop() noexcept;

    std::expected<std::shared_ptr<Machine>, GenericError> spawn(SpawnOptions options) noexcept;

    static std::expected<Hypervisor, GenericError> connect(const std::string& uri) noexcept;

private:
    std::unique_ptr<HypervisorImpl> m_impl;
};

} // namespace ls_gitea_runner::libvirt
