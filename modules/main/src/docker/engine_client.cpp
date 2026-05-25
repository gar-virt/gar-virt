#include "engine_client.hpp"

#include <utility/string.hpp>

#include <boost/json.hpp>

#include <cstdio>

namespace ls_gitea_runner {

DockerEngineClient::DockerEngineClient() {}

DockerEngineClient::~DockerEngineClient() {}

std::expected<DockerContainerId, GenericError>
DockerEngineClient::container_create(const std::string& name, const std::string& image,
                                     const std::vector<std::string>& cmd) const {
    std::vector<std::string> docker_cmd = {"docker", "container", "create", "--rm", "--name", name, image};
    std::copy(cmd.begin(), cmd.end(), std::back_inserter(docker_cmd));
    auto res{utility::spawn_cmd(docker_cmd)};
    if (!res) {
        return std::unexpected{GenericError{res.error().what()}};
    }
    utility::string_trim_right(std::in_place, res->output);
    return DockerContainerId{std::move(res->output)};
}

std::expected<int, GenericError> DockerEngineClient::container_exec(const DockerContainerId& id,
                                                                    const std::vector<std::string>& cmd,
                                                                    utility::SpawnOptions options) const {
    std::vector<std::string> docker_cmd = {"docker", "container", "exec", "--interactive", id.value};
    std::copy(cmd.begin(), cmd.end(), std::back_inserter(docker_cmd));
    auto res{utility::spawn_cmd(docker_cmd, std::move(options))};
    if (!res) {
        return std::unexpected{GenericError{res.error().what()}};
    }
    return *res;
}

std::expected<utility::SpawnResult, GenericError>
DockerEngineClient::container_exec(const DockerContainerId& id, const std::vector<std::string>& cmd) const {
    utility::SpawnResult result;
    utility::SpawnOptions options{.stdout_reader = [&](const char* buffer, int length) {
        result.output.append(buffer, static_cast<size_t>(length));
        return length;
    }};
    if (auto res{container_exec(id, cmd, std::move(options))}) {
        result.exit_code = *res;
        return result;
    } else {
        return std::unexpected{res.error()};
    }
}

std::expected<void, GenericError> DockerEngineClient::container_start(const DockerContainerId& id) const {
    const std::vector<std::string> docker_cmd = {"docker", "container", "start", id.value};
    auto res{utility::spawn_cmd(docker_cmd)};
    if (!res) {
        return std::unexpected{GenericError{res.error().what()}};
    }
    return {};
}

std::expected<void, GenericError> DockerEngineClient::container_kill(const DockerContainerId& id) const {
    const std::vector<std::string> docker_cmd = {"docker", "container", "kill", id.value};
    auto res{utility::spawn_cmd(docker_cmd)};
    if (!res) {
        return std::unexpected{GenericError{res.error().what()}};
    }
    return {};
}

std::expected<int, GenericError> DockerEngineClient::container_run(const std::string& name, const std::string& image,
                                                                   const std::vector<std::string>& cmd,
                                                                   utility::SpawnOptions options) const {
    std::vector<std::string> docker_cmd = {"docker", "container", "run", "--interactive",
                                           "--rm",   "--name",    name,  image};
    std::copy(cmd.begin(), cmd.end(), std::back_inserter(docker_cmd));
    auto res{utility::spawn_cmd(docker_cmd, std::move(options))};
    if (!res) {
        return std::unexpected{GenericError{res.error().what()}};
    }
    return {};
}

std::expected<utility::SpawnResult, GenericError>
DockerEngineClient::container_run(const std::string& name, const std::string& image,
                                  const std::vector<std::string>& cmd) const {
    utility::SpawnResult result;
    utility::SpawnOptions options{.stdout_reader = [&](const char* buffer, int length) {
        result.output.append(buffer, static_cast<size_t>(length));
        return length;
    }};
    if (auto res{container_run(name, image, cmd, std::move(options))}) {
        result.exit_code = *res;
        return result;
    } else {
        return std::unexpected{res.error()};
    }
}

std::expected<bool, GenericError> DockerEngineClient::container_is_running(const DockerContainerId& id) const {
    const std::vector<std::string> docker_cmd = {"docker", "container", "inspect", "--format", "json", id.value};
    auto res{utility::spawn_cmd(docker_cmd)};
    if (!res) {
        return std::unexpected{GenericError{res.error().what()}};
    }
    try {
        const auto j{boost::json::parse(std::move(res->output))};
        const auto& j_items{j.as_array()};
        if (j_items.empty()) {
            return std::unexpected{GenericError{std::format("No such container: {}", id.value)}};
        }
        const auto& j_first_item = j_items.at(0).as_object();
        const auto& j_state{j_first_item.at("State").as_object()};
        const auto& j_status{j_state.at("Status").as_string()};
        if (j_status == "running") {
            return true;
        }
        if (j_status == "created" || j_status == "paused" || j_status == "restarting" || j_status == "exited") {
            return false;
        }
        return std::unexpected{std::format("Container {} is in an invalid state: {}", id.value, std::string{j_status})};
    } catch (const boost::system::system_error& ex) {
        return std::unexpected{std::format("JSON parse failed: {}", ex.what())};
    }
}

std::expected<void, GenericError> DockerEngineClient::container_cp_into(const DockerContainerId& id,
                                                                        const std::filesystem::path& local_path,
                                                                        const std::string& remote_path) const {
    const std::vector<std::string> docker_cmd = {"docker",
                                                 "container",
                                                 "cp",
                                                 "--quiet",
                                                 utility::string_from_u8string(local_path.u8string()),
                                                 std::format("{}:{}", id.value, remote_path)};
    auto res{utility::spawn_cmd(docker_cmd)};
    if (!res) {
        return std::unexpected{GenericError{res.error().what()}};
    }
    return {};
}

} // namespace ls_gitea_runner
