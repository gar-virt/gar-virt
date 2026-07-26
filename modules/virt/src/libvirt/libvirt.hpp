#pragma once

#include <utility/result.hpp>

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
    Result<void> wait();
    Result<void> wait_for_guest_agent();

    Result<void> write_file(const std::string& file_path, std::span<const std::byte> content);
    Result<SpawnResult> shell_exec(const std::vector<std::string>& cmd,
                                   const std::optional<std::chrono::seconds>& timeout);

    Result<void> resume();
    Result<void> kill();
    Result<bool> is_ready() const;

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

    // Result<void> run() noexcept;
    // void stop() noexcept;

    Result<std::shared_ptr<Machine>> spawn(const SpawnOptions& options);

    static Result<Hypervisor> connect(const std::string& uri);

private:
    std::unique_ptr<HypervisorImpl> m_impl;
};

} // namespace gv::libvirt
