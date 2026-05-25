#pragma once

#include "../error.hpp"
#include "docker_container_id.hpp"

#include <utility/file_handle.hpp>
#include <utility/spawn.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace ls_gitea_runner {

class DockerEngineClient final {
public:
    DockerEngineClient();
    ~DockerEngineClient();

    DockerEngineClient(const DockerEngineClient&) = delete;
    DockerEngineClient& operator=(const DockerEngineClient&) = delete;
    DockerEngineClient(DockerEngineClient&&) = default;
    DockerEngineClient& operator=(DockerEngineClient&&) = default;

    std::expected<DockerContainerId, GenericError> container_create(const std::string& name, const std::string& image,
                                                                    const std::vector<std::string>& cmd) const;

    std::expected<int, GenericError> container_exec(const DockerContainerId& id, const std::vector<std::string>& cmd,
                                                    utility::SpawnOptions options) const;

    std::expected<utility::SpawnResult, GenericError> container_exec(const DockerContainerId& id,
                                                                     const std::vector<std::string>& cmd) const;

    std::expected<void, GenericError> container_start(const DockerContainerId& id) const;
    std::expected<void, GenericError> container_kill(const DockerContainerId& id) const;

    std::expected<int, GenericError> container_run(const std::string& name, const std::string& image,
                                                   const std::vector<std::string>& cmd,
                                                   utility::SpawnOptions options) const;

    std::expected<utility::SpawnResult, GenericError> container_run(const std::string& name, const std::string& image,
                                                                    const std::vector<std::string>& cmd) const;

    std::expected<bool, GenericError> container_is_running(const DockerContainerId& id) const;

    std::expected<void, GenericError> container_cp_into(const DockerContainerId& id,
                                                        const std::filesystem::path& local_path,
                                                        const std::string& remote_path) const;
};

} // namespace ls_gitea_runner
