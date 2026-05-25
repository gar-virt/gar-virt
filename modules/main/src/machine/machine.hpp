#pragma once

#include "../error.hpp"

#include <utility/spawn.hpp>

#include <chrono>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace ls_gitea_runner {

class machine {
public:
    struct info_t {
        std::string os;
        std::string arch;
        std::string temp_dir;
    };

    virtual ~machine() = default;
    virtual const std::string& get_id() const = 0;
    virtual std::expected<void, generic_error> terminate() = 0;
    virtual std::expected<int, generic_error> shell_exec(const std::vector<std::string>& cmd,
                                                         utility::spawn_options options) const = 0;
    virtual std::expected<utility::spawn_result, generic_error>
    shell_exec(const std::vector<std::string>& cmd) const = 0;
    virtual bool wait_until_ready(std::chrono::seconds timeout) = 0;
    virtual std::expected<void, generic_error> copy_file_into(const std::filesystem::path& local_path,
                                                              const std::string& remote_path) = 0;
    virtual const info_t& info() const = 0;
};

} // namespace ls_gitea_runner
