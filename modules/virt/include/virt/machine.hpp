#pragma once

#include <virt/arch.hpp>

#include <utility/concepts.hpp>
#include <utility/result.hpp>
#include <utility/shutdown_signal.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace gv::virt {

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
    virtual Result<void> write_file_impl(const std::string& remote_path, std::span<const std::byte> content) = 0;
};

} // namespace gv::virt
