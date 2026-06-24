#pragma once

#include "../error.hpp"

#include <utility/spawn.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ls_gitea_runner {

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
    virtual std::expected<int, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                        utility::SpawnOptions options) const = 0;
    virtual std::expected<utility::SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd) const = 0;
    virtual bool wait_until_ready(std::chrono::seconds timeout) = 0;
    virtual std::expected<void, GenericError> copy_file_into(const std::filesystem::path& local_path,
                                                             const std::string& remote_path) = 0;
    virtual std::expected<void, GenericError> write_file(const std::string& remote_path,
                                                         std::span<const std::byte> content) = 0;
    virtual const Info& info() const = 0;
};

} // namespace ls_gitea_runner
