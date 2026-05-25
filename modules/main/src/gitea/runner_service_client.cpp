#include "runner_service_client.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ls_gitea_runner::gitea {

template <typename T> std::expected<std::vector<std::byte>, GenericError> encode_payload(const T& msg) noexcept {
    const auto byte_size{msg.ByteSizeLong()};
    std::vector<std::byte> data;
    data.resize(static_cast<std::size_t>(byte_size));
    if (!msg.SerializeToArray(data.data(), byte_size)) {
        return std::unexpected{GenericError{"Failed to encode gRPC message"}};
    }
    return data;
}

template <typename T> std::expected<T, GenericError> decode_payload(const std::vector<std::byte>& payload) noexcept {
    T msg;
    if (!msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        return std::unexpected{GenericError{"Failed to decode gRPC message"}};
    }
    return msg;
}

template <typename Request, typename Response>
std::expected<Response, GenericError> send_post_request(const HttpClient& client, const std::string& path,
                                                        const Request& req) noexcept {
    return encode_payload(req)
        .and_then([&](auto payload) { return client.post(path, payload); })
        .and_then([](auto res) { return decode_payload<Response>(res.body); });
}

GiteaRunnerServiceClient::GiteaRunnerServiceClient(const std::string& instance_url) : m_client{instance_url} {
    m_client.add_request_middleware([this](auto& req) {
        if (req.method == HttpMethod::post) {
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
GiteaRunnerServiceClient::ping(::ping::v1::PingRequest req) const noexcept {
    return send_post_request<::ping::v1::PingRequest, ::ping::v1::PingResponse>(m_client, "/ping.v1.PingService/Ping",
                                                                                req);
}

std::expected<::runner::v1::RegisterResponse, GenericError>
GiteaRunnerServiceClient::register_(::runner::v1::RegisterRequest req) const noexcept {
    return send_post_request<::runner::v1::RegisterRequest, ::runner::v1::RegisterResponse>(
        m_client, "/runner.v1.RunnerService/Register", req);
}

std::expected<::runner::v1::DeclareResponse, GenericError>
GiteaRunnerServiceClient::declare(::runner::v1::DeclareRequest req) const noexcept {
    return send_post_request<::runner::v1::DeclareRequest, ::runner::v1::DeclareResponse>(
        m_client, "/runner.v1.RunnerService/Declare", req);
}

std::expected<::runner::v1::FetchTaskResponse, GenericError>
GiteaRunnerServiceClient::fetch_task(::runner::v1::FetchTaskRequest req) const noexcept {
    return send_post_request<::runner::v1::FetchTaskRequest, ::runner::v1::FetchTaskResponse>(
        m_client, "/runner.v1.RunnerService/FetchTask", req);
}

std::expected<::runner::v1::UpdateTaskResponse, GenericError>
GiteaRunnerServiceClient::update_task(::runner::v1::UpdateTaskRequest req) const noexcept {
    return send_post_request<::runner::v1::UpdateTaskRequest, ::runner::v1::UpdateTaskResponse>(
        m_client, "/runner.v1.RunnerService/UpdateTask", req);
}

std::expected<::runner::v1::UpdateLogResponse, GenericError>
GiteaRunnerServiceClient::update_log(::runner::v1::UpdateLogRequest req) const noexcept {
    return send_post_request<::runner::v1::UpdateLogRequest, ::runner::v1::UpdateLogResponse>(
        m_client, "/runner.v1.RunnerService/UpdateLog", req);
}

} // namespace ls_gitea_runner::gitea
