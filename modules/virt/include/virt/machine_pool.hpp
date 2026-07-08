#pragma once

#include "machine.hpp"

#include <utility/error.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>

namespace ls_gitea_runner {

struct MachinePoolStats {
    size_t active{};
    size_t idle{};
    size_t warming{};

    std::strong_ordering operator<=>(const MachinePoolStats&) const = default;
};

class MachinePool final {
public:
    MachinePool(size_t idle_target, size_t max_concurrency,
                std::move_only_function<std::expected<std::unique_ptr<Machine>, GenericError>()> machine_spawner);
    ~MachinePool();
    MachinePool(const MachinePool&) = delete;
    MachinePool(MachinePool&&);
    MachinePool& operator=(const MachinePool&) = delete;
    MachinePool& operator=(MachinePool&&);
    std::expected<std::shared_ptr<Machine>, GenericError> acquire(std::chrono::milliseconds timeout) noexcept;
    void release(std::shared_ptr<Machine> machine) noexcept;
    void start();
    void set_stats_callback(std::move_only_function<void(MachinePoolStats)> cb) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner
