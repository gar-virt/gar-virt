#pragma once

#include <utility/error.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace gv::libvirt {

struct SpawnOptions {
    std::string volume;
    std::string domain;
    std::string storage_pool;
};

struct SpawnResult {
    int exit_code{};
    std::string output;
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

    const std::string& get_name() const noexcept;
    std::expected<void, Error> wait();
    std::expected<void, Error> wait_for_guest_agent();

    std::expected<void, Error> write_file(const std::string& file_path, std::span<const std::byte> content);
    std::expected<SpawnResult, Error> shell_exec(const std::vector<std::string>& cmd,
                                                        const std::optional<std::chrono::seconds>& timeout);

    std::expected<void, Error> resume();
    std::expected<void, Error> kill();
    std::expected<bool, Error> is_ready() const;

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

    // std::expected<void, Error> run() noexcept;
    // void stop() noexcept;

    std::expected<std::shared_ptr<Machine>, Error> spawn(const SpawnOptions& options);

    static std::expected<Hypervisor, Error> connect(const std::string& uri);

private:
    std::unique_ptr<HypervisorImpl> m_impl;
};

} // namespace gv::libvirt
