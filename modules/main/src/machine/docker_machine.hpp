#pragma once

#include "docker_container_id.hpp"
#include "machine.hpp"

#include <memory>

namespace ls_gitea_runner {

class docker_machine final : public machine {
public:
    docker_machine(const docker_container_id& id, info_t info);
    ~docker_machine();

    const std::string& get_id() const override;
    std::expected<void, generic_error> terminate() override;
    std::expected<int, generic_error> shell_exec(const std::vector<std::string>& cmd,
                                                 utility::spawn_options options) const override;
    std::expected<utility::spawn_result, generic_error> shell_exec(const std::vector<std::string>& cmd) const override;
    bool wait_until_ready(std::chrono::seconds timeout) override;
    std::expected<void, generic_error> copy_file_into(const std::filesystem::path& local_path,
                                                      const std::string& remote_path) override;
    const info_t& info() const override;

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace ls_gitea_runner
