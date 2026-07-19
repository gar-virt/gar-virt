#include <gitea/runner_service_client.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace ls_gitea_runner::gitea {

template <typename T> std::expected<std::vector<std::byte>, GenericError> encode_payload(const T& msg) {
    const auto byte_size{msg.ByteSizeLong()};
    std::vector<std::byte> data;
    data.resize(static_cast<std::size_t>(byte_size));
    if (!msg.SerializeToArray(data.data(), byte_size)) {
        return std::unexpected{GenericError{"Failed to encode gRPC message"}};
    }
    return data;
}

template <typename T> std::expected<T, GenericError> decode_payload(const std::vector<std::byte>& payload) {
    T msg;
    if (!msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        return std::unexpected{GenericError{"Failed to decode gRPC message"}};
    }
    return msg;
}

#define DECLARE_PAYLOAD_ENCODING_FN(T)                                                                                 \
    template std::expected<std::vector<std::byte>, GenericError> encode_payload<T>(const T& msg);                      \
    template std::expected<T, GenericError> decode_payload<T>(const std::vector<std::byte>& payload);

DECLARE_PAYLOAD_ENCODING_FN(::ping::v1::PingRequest)
DECLARE_PAYLOAD_ENCODING_FN(::ping::v1::PingResponse)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::DeclareRequest)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::DeclareResponse)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::FetchTaskRequest)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::FetchTaskResponse)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::LogRow)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::RegisterRequest)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::RegisterResponse)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::Runner)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::StepState)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::Task)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::TaskNeed)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::TaskState)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::UpdateLogRequest)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::UpdateLogResponse)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::UpdateTaskRequest)
DECLARE_PAYLOAD_ENCODING_FN(::runner::v1::UpdateTaskResponse)

template <typename Request, typename Response>
std::expected<Response, GenericError> send_post_request(const utility::HttpClient& client, const std::string& path,
                                                        const Request& req) {
    return encode_payload(req)
        .and_then([&](auto payload) { return client.post(path, payload); })
        .and_then([](auto res) { return decode_payload<Response>(res.body); });
}

GiteaRunnerServiceClient::GiteaRunnerServiceClient(const std::string& instance_url)
        : m_client{utility::http_path_join(instance_url, "/api/actions")} {
    m_client.add_request_middleware([this](auto& req) {
        if (req.method == utility::HttpMethod::post) {
            req.headers.emplace("Content-Type", "application/proto");
        }

        req.headers.emplace("Accept", "application/proto");

        if (!m_credentials.uuid.empty()) {
            req.headers.emplace("X-Runner-UUID", m_credentials.uuid);
        }

        if (!m_credentials.token.empty()) {
            req.headers.emplace("X-Runner-Token", m_credentials.token);
        }

        return true;
    });
}

void GiteaRunnerServiceClient::set_credentials(GiteaRunnerCredentials credentials) {
    m_credentials = std::move(credentials);
}

std::expected<::ping::v1::PingResponse, GenericError>
GiteaRunnerServiceClient::ping(::ping::v1::PingRequest req) const {
    return send_post_request<::ping::v1::PingRequest, ::ping::v1::PingResponse>(m_client, "/ping.v1.PingService/Ping",
                                                                                req);
}

std::expected<::runner::v1::RegisterResponse, GenericError>
GiteaRunnerServiceClient::register_(::runner::v1::RegisterRequest req) const {
    return send_post_request<::runner::v1::RegisterRequest, ::runner::v1::RegisterResponse>(
        m_client, "/runner.v1.RunnerService/Register", req);
}

std::expected<::runner::v1::DeclareResponse, GenericError>
GiteaRunnerServiceClient::declare(::runner::v1::DeclareRequest req) const {
    return send_post_request<::runner::v1::DeclareRequest, ::runner::v1::DeclareResponse>(
        m_client, "/runner.v1.RunnerService/Declare", req);
}

std::expected<::runner::v1::FetchTaskResponse, GenericError>
GiteaRunnerServiceClient::fetch_task(::runner::v1::FetchTaskRequest req) const {
    return send_post_request<::runner::v1::FetchTaskRequest, ::runner::v1::FetchTaskResponse>(
        m_client, "/runner.v1.RunnerService/FetchTask", req);
}

std::expected<::runner::v1::UpdateTaskResponse, GenericError>
GiteaRunnerServiceClient::update_task(::runner::v1::UpdateTaskRequest req) const {
    return send_post_request<::runner::v1::UpdateTaskRequest, ::runner::v1::UpdateTaskResponse>(
        m_client, "/runner.v1.RunnerService/UpdateTask", req);
}

std::expected<::runner::v1::UpdateLogResponse, GenericError>
GiteaRunnerServiceClient::update_log(::runner::v1::UpdateLogRequest req) const {
    return send_post_request<::runner::v1::UpdateLogRequest, ::runner::v1::UpdateLogResponse>(
        m_client, "/runner.v1.RunnerService/UpdateLog", req);
}

} // namespace ls_gitea_runner::gitea
