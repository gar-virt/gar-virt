#include <gitea/runner.hpp>

#include <gitea/admin_service_client.hpp>
#include <gitea/runner_service_client.hpp>
#include <ping/v1/messages.pb.h>
#include <runner/v1/messages.pb.h>

#include <utility/log/global_logger.hpp>

#include <expected>
#include <format>
#include <memory>
#include <string>

namespace ls_gitea_runner::gitea {
namespace {

std::expected<::ping::v1::PingResponse, GenericError> ping_internal(const gitea::GiteaRunnerServiceClient& client,
                                                                    const RunnerOptions& options) {
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
                  const std::string& reg_token) {
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
declare_internal(const gitea::GiteaRunnerServiceClient& client, const RunnerOptions& options) {
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
fetch_task_internal(const gitea::GiteaRunnerServiceClient& client) {
    auto fetch_task_request{::runner::v1::FetchTaskRequest{}};
    auto fetch_task_response = client.fetch_task(fetch_task_request);
    if (!fetch_task_response) {
        return std::unexpected{
            GenericError{std::format("Failed to fetch any new tasks: {}", fetch_task_response.error().what())}};
    }
    return *fetch_task_response;
}

} // namespace

Runner::Runner(int64_t id, std::vector<std::string> labels, std::string forge_uri,
               gitea::GiteaRunnerCredentials credentials, std::shared_ptr<gitea::GiteaRunnerServiceClient> client,
               std::shared_ptr<gitea::AdminServiceClient> admin)
        : m_id{id}, m_labels{std::move(labels)}, m_forge_uri{std::move(forge_uri)},
          m_credentials(std::move(credentials)), m_client{std::move(client)}, m_admin{std::move(admin)} {}

Runner::~Runner() {
    if (!m_moved) {
        global_logger().debug("Unregistering runner with ID {}.", m_id);
        if (auto res{m_admin->remove_runner(m_id)}; !res) {
            global_logger().error("Failed to unregister runner with ID {}: {}", m_id, res.error().what());
        }
    }
}

Runner::Runner(Runner&& other) noexcept
        : m_id{other.m_id}, m_labels{std::move(other.m_labels)}, m_forge_uri{std::move(other.m_forge_uri)},
          m_credentials{std::move(other.m_credentials)}, m_client{std::move(other.m_client)},
          m_admin{std::move(other.m_admin)} {
    other.m_moved = true;
}

Runner& Runner::operator=(Runner&& other) noexcept {
    if (this != &other) {
        m_id = other.m_id;
        m_labels = std::move(other.m_labels);
        m_forge_uri = std::move(other.m_forge_uri);
        m_credentials = std::move(other.m_credentials);
        m_client = std::move(other.m_client);
        m_admin = std::move(other.m_admin);
        other.m_moved = true;
    }
    return *this;
}

std::expected<Runner, GenericError> Runner::connect(const RunnerOptions& options,
                                                    std::shared_ptr<gitea::AdminServiceClient> admin) {
    const auto reg_token(admin->get_registration_token());
    if (!reg_token) {
        return std::unexpected{reg_token.error()};
    }

    auto client{std::make_shared<gitea::GiteaRunnerServiceClient>(options.forge_uri)};

    auto ping_res{ping_internal(*client, options)};
    if (!ping_res) {
        return std::unexpected{ping_res.error()};
    }

    auto register_res{register_internal(*client, options, *reg_token)};
    if (!register_res) {
        return std::unexpected{register_res.error()};
    }

    const auto& runner{register_res->runner()};
    global_logger().debug("Registered runner with ID {}.", runner.id());

    gitea::GiteaRunnerCredentials credentials{.uuid = runner.uuid(), .token = runner.token()};
    client->set_credentials(credentials);

    auto declare_res{declare_internal(*client, options)};
    if (!declare_res) {
        return std::unexpected{declare_res.error()};
    }

    return Runner{runner.id(),       std::move(options.labels), std::move(options.forge_uri), std::move(credentials),
                  std::move(client), std::move(admin)};
}

std::expected<::runner::v1::FetchTaskResponse, GenericError> Runner::fetch_task() const {
    return fetch_task_internal(*m_client);
}

int64_t Runner::id() const noexcept { return m_id; }
const gitea::GiteaRunnerCredentials& Runner::credentials() const noexcept { return m_credentials; }
const gitea::GiteaRunnerServiceClient& Runner::client() const noexcept { return *m_client; }
const std::vector<std::string>& Runner::labels() const noexcept { return m_labels; }
const std::string& Runner::forge_uri() const noexcept { return m_forge_uri; }

void Runner::set_task_failed(const ::runner::v1::Task& task) {
    ::runner::v1::UpdateTaskRequest update_req;
    auto& task_state{*update_req.mutable_state()};
    task_state.set_id(task.id());
    task_state.set_result(::runner::v1::RESULT_FAILURE);
    std::ignore = m_client->update_task(update_req);
}

} // namespace ls_gitea_runner::gitea
