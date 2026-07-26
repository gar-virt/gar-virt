#pragma once

#include <utility/concepts.hpp>
#include <utility/result.hpp>
#include <utility/shutdown_signal.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gv::virt {

struct Arch {
    enum Type { amd64, arm64 };

    static Result<Arch::Type> from_name(std::string_view name) noexcept;
    static std::string to_name(Arch::Type value);
};

struct SpawnResult {
    int exit_code{};
    std::string output;
};

class Machine {
public:
    struct Info {
        std::string os;
        Arch::Type arch;
        std::string temp_dir;
    };

    virtual ~Machine() = default;
    Machine(const Machine&) = delete;
    Machine(Machine&&) noexcept = delete;
    Machine& operator=(const Machine&) = delete;
    Machine& operator=(Machine&&) noexcept = delete;
    virtual const std::string& get_id() const = 0;
    virtual Result<void> terminate() = 0;
    virtual Result<SpawnResult> shell_exec(const std::vector<std::string>& cmd,
                                           const std::optional<std::chrono::seconds>& timeout) const = 0;
    virtual Result<void> wait_for_guest_agent(std::chrono::seconds timeout, const utility::ShutdownSignal& stop) = 0;
    virtual const Info& info() const = 0;

    std::string make_temp_path(const std::string& sub_path) const;

    template <utility::contiguous_byte_container T>
    Result<void> write_file(const std::string& remote_path, const T& content) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return write_file_impl(remote_path, {reinterpret_cast<const std::byte*>(content.data()), content.size()});
    }

protected:
    Machine() = default;
    virtual Result<void> write_file_impl(const std::string& remote_path, std::span<const std::byte> content) = 0;
};

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

class Backend {
public:
    virtual ~Backend() = default;
    Backend(const Backend&) = delete;
    Backend(Backend&&) noexcept = delete;
    Backend& operator=(const Backend&) = delete;
    Backend& operator=(Backend&&) noexcept = delete;
    virtual Result<std::unique_ptr<Machine>> spawn(const Machine::Info& info,
                                                   const std::string& serialized_pool_details,
                                                   const std::string& serialized_template_details,
                                                   const std::filesystem::path& config_dir) = 0;

protected:
    Backend() = default;
};

Result<std::unique_ptr<Backend>> create_backend(const std::string& name);

} // namespace gv::virt
