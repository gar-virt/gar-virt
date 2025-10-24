#pragma once

#include "../error.hpp"
#include "docker_container_id.hpp"

#include <utility/file_handle.hpp>
#include <utility/spawn.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace ls_gitea_runner {

class docker_engine_client final {
public:
    docker_engine_client();
    ~docker_engine_client();

    docker_engine_client(const docker_engine_client&) = delete;
    docker_engine_client& operator=(const docker_engine_client&) = delete;
    docker_engine_client(docker_engine_client&&) = default;
    docker_engine_client& operator=(docker_engine_client&&) = default;

    std::expected<docker_container_id, generic_error>
    container_create(const std::string& name, const std::string& image, const std::vector<std::string>& cmd) const;

    std::expected<int, generic_error> container_exec(const docker_container_id& id, const std::vector<std::string>& cmd,
                                                     utility::spawn_options options) const;

    std::expected<utility::spawn_result, generic_error> container_exec(const docker_container_id& id,
                                                                       const std::vector<std::string>& cmd) const;

    std::expected<void, generic_error> container_start(const docker_container_id& id) const;
    std::expected<void, generic_error> container_kill(const docker_container_id& id) const;

    std::expected<int, generic_error> container_run(const std::string& name, const std::string& image,
                                                    const std::vector<std::string>& cmd,
                                                    utility::spawn_options options) const;

    std::expected<utility::spawn_result, generic_error> container_run(const std::string& name, const std::string& image,
                                                                      const std::vector<std::string>& cmd) const;

    std::expected<bool, generic_error> container_is_running(const docker_container_id& id) const;

    std::expected<void, generic_error> container_cp_into(const docker_container_id& id,
                                                         const std::filesystem::path& local_path,
                                                         const std::string& remote_path) const;
};

} // namespace ls_gitea_runner
