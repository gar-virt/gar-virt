#include "gitea/runner.hpp"

#include "ping/v1/messages.pb.h"
#include "runner/v1/messages.pb.h"

#include "gitea/admin_service_client.hpp"
#include "gitea/runner_service_client.hpp"

#include "utility/log/global_logger.hpp"

#include <expected>
#include <format>
#include <memory>
#include <string>

namespace ls_gitea_runner::gitea {

std::expected<::ping::v1::PingResponse, GenericError> ping_internal(const gitea::GiteaRunnerServiceClient& client,
                                                                    const RunnerOptions& options) noexcept {
    auto ping_request{::ping::v1::PingRequest{}};
    ping_request.set_data(options.name);
    auto ping_response{client.ping(ping_request)};
    if (!ping_response) {
        return std::unexpected{GenericError{std::format("Failed to send ping: {}", ping_response.error().what())}};
    }
    return *ping_response;
}

std::expected<::runner::v1::RegisterResponse, GenericError>
register_internal(const gitea::GiteaRunnerServiceClient& client, const RunnerOptions& options,
                  const std::string& reg_token) noexcept {
    auto reqister_request{::runner::v1::RegisterRequest{}};
    reqister_request.set_name(options.name);
    reqister_request.set_token(reg_token);
    reqister_request.set_version(options.version);
    for (auto& label : options.get_label_names()) {
        reqister_request.add_labels(label);
    }
    reqister_request.set_ephemeral(true);
    auto register_response{client.register_(reqister_request)};
    if (!register_response) {
        return std::unexpected{
            GenericError{std::format("Failed to register runner: {}", register_response.error().what())}};
    }
    return *register_response;
}

std::expected<::runner::v1::DeclareResponse, GenericError>
declare_internal(const gitea::GiteaRunnerServiceClient& client, const RunnerOptions& options) noexcept {
    auto declare_request{::runner::v1::DeclareRequest{}};
    declare_request.set_version(options.version);
    for (auto& label : options.get_label_names()) {
        declare_request.add_labels(label);
    }
    auto declare_response{client.declare(declare_request)};
    if (!declare_response) {
        return std::unexpected{
            GenericError{std::format("Failed to declare runner: {}", declare_response.error().what())}};
    }
    return declare_response;
}

std::expected<::runner::v1::FetchTaskResponse, GenericError>
fetch_task_internal(const gitea::GiteaRunnerServiceClient& client) noexcept {
    auto fetch_task_request{::runner::v1::FetchTaskRequest{}};
    auto fetch_task_response = client.fetch_task(fetch_task_request);
    if (!fetch_task_response) {
        return std::unexpected{GenericError{"Failed to fetch any new tasks"}};
    }
    return *fetch_task_response;
}

Runner::Runner(int64_t id, gitea::GiteaRunnerCredentials credentials,
               std::shared_ptr<gitea::GiteaRunnerServiceClient> client,
               std::shared_ptr<gitea::AdminServiceClient> admin)
        : m_id{id}, m_credentials(std::move(credentials)), m_client{std::move(client)}, m_admin{std::move(admin)} {}

Runner::~Runner() {
    if (!m_moved) {
        global_logger().verbose("Unregistering runner with ID {}.", m_id);
        if (auto res{m_admin->remove_runner(m_id)}; !res) {
            global_logger().error("Failed to unregister runner with ID {}: {}", m_id, res.error().what());
        }
    }
}

Runner::Runner(Runner&& other) noexcept
        : m_id{other.m_id}, m_credentials{std::move(other.m_credentials)}, m_client{std::move(other.m_client)},
          m_admin{std::move(other.m_admin)} {
    other.m_moved = true;
}

Runner& Runner::operator=(Runner&& other) noexcept {
    if (this != &other) {
        m_id = other.m_id;
        m_credentials = std::move(other.m_credentials);
        m_client = std::move(other.m_client);
        m_admin = std::move(other.m_admin);
        other.m_moved = true;
    }
    return *this;
}

std::expected<Runner, GenericError> Runner::connect(const RunnerOptions& options,
                                                    std::shared_ptr<gitea::AdminServiceClient> admin) noexcept {
    const auto reg_token(admin->get_registration_token());
    if (!reg_token) {
        return std::unexpected{reg_token.error()};
    }

    auto client{std::make_shared<gitea::GiteaRunnerServiceClient>(options.instance_url)};

    auto ping_res{ping_internal(*client, options)};
    if (!ping_res) {
        return std::unexpected{ping_res.error()};
    }

    auto register_res{register_internal(*client, options, *reg_token)};
    if (!register_res) {
        return std::unexpected{register_res.error()};
    }

    auto& runner{register_res->runner()};
    global_logger().verbose("Registered runner with ID {}.", runner.id());

    gitea::GiteaRunnerCredentials credentials{.uuid = runner.uuid(), .token = runner.token()};
    client->set_credentials(credentials);

    auto declare_res{declare_internal(*client, options)};
    if (!declare_res) {
        return std::unexpected{declare_res.error()};
    }

    return Runner{runner.id(), std::move(credentials), std::move(client), std::move(admin)};
}

std::expected<::runner::v1::FetchTaskResponse, GenericError> Runner::fetch_task() noexcept {
    return fetch_task_internal(*m_client);
}

int64_t Runner::id() const noexcept { return m_id; }
const gitea::GiteaRunnerCredentials& Runner::credentials() const noexcept { return m_credentials; }
const gitea::GiteaRunnerServiceClient& Runner::client() const noexcept { return *m_client; }

} // namespace ls_gitea_runner::gitea
