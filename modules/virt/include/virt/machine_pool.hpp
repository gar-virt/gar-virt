#pragma once

#include "machine.hpp"

#include <utility/result.hpp>
#include <utility/shutdown_signal.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>

namespace gv::virt {

struct MachinePoolStats {
    size_t provisioned{};
    size_t acquiring{};
    size_t acquired{};
    size_t active{};
    size_t idle{};
    size_t warming{};

    std::strong_ordering operator<=>(const MachinePoolStats&) const = default;
};

class MachinePool final {
public:
    MachinePool(size_t idle_target, size_t max_concurrency,
                std::move_only_function<Result<std::unique_ptr<Machine>>()> machine_spawner,
                utility::ShutdownSignal shutdown_signal);
    ~MachinePool();
    MachinePool(const MachinePool&) = delete;
    MachinePool(MachinePool&&) noexcept;
    MachinePool& operator=(const MachinePool&) = delete;
    MachinePool& operator=(MachinePool&&) noexcept;
    Result<std::shared_ptr<Machine>> acquire(std::chrono::milliseconds timeout);
    void activate(std::shared_ptr<Machine> machine);
    void deactivate(std::shared_ptr<Machine> machine);
    void release(std::shared_ptr<Machine> machine);
    void start();
    void stop();
    void set_stats_callback(std::move_only_function<void(const MachinePoolStats&) noexcept> cb) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace gv::virt
