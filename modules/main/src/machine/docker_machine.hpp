#pragma once

#include "docker_container_id.hpp"
#include "machine.hpp"

#include <memory>

namespace ls_gitea_runner {

class DockerMachine final : public Machine {
public:
    DockerMachine(const DockerContainerId& id, Info info);
    ~DockerMachine();

    const std::string& get_id() const override;
    std::expected<void, GenericError> terminate() override;
    std::expected<int, GenericError> shell_exec(const std::vector<std::string>& cmd,
                                                utility::SpawnOptions options) const override;
    std::expected<utility::SpawnResult, GenericError> shell_exec(const std::vector<std::string>& cmd) const override;
    bool wait_until_ready(std::chrono::seconds timeout) override;
    std::expected<void, GenericError> copy_file_into(const std::filesystem::path& local_path,
                                                     const std::string& remote_path) override;
    const Info& info() const override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner
