#pragma once

#include <utility/error.hpp>
#include <utility/shutdown_signal.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ls_gitea_runner {

struct SpawnResult {
    int exit_code{};
    std::string output;
};

class Machine {
public:
    struct Info {
        std::string os;
        std::string arch;
        std::string temp_dir;
    };

    virtual ~Machine() = default;
    virtual const std::string& get_id() const = 0;
    virtual std::expected<void, GenericError> terminate() = 0;
    virtual std::expected<SpawnResult, GenericError>
    shell_exec(const std::vector<std::string>& cmd, const std::optional<std::chrono::seconds>& timeout) const = 0;
    virtual std::expected<void, GenericError> wait_for_guest_agent(std::chrono::seconds timeout,
                                                                   utility::ShutdownSignal stop) = 0;
    virtual std::expected<void, GenericError> copy_file_into(const std::filesystem::path& local_path,
                                                             const std::string& remote_path) = 0;
    virtual std::expected<void, GenericError> write_file(const std::string& remote_path,
                                                         std::span<const std::byte> content) = 0;
    virtual const Info& info() const = 0;

    std::string make_temp_path(const std::string& sub_path) const;
};

} // namespace ls_gitea_runner
