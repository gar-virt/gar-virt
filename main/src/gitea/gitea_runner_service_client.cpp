#include "gitea_runner_service_client.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ls_gitea_runner::gitea {

template <typename T> std::expected<std::vector<std::byte>, generic_error> encode_payload(const T& msg) noexcept {
    const auto byte_size{msg.ByteSizeLong()};
    std::vector<std::byte> data;
    data.resize(static_cast<std::size_t>(byte_size));
    if (!msg.SerializeToArray(data.data(), byte_size)) {
        return std::unexpected{generic_error{"Failed to encode gRPC message"}};
    }
    return data;
}

template <typename T> std::expected<T, generic_error> decode_payload(const std::vector<std::byte>& payload) noexcept {
    T msg;
    if (!msg.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        return std::unexpected{generic_error{"Failed to decode gRPC message"}};
    }
    return msg;
}

template <typename Request, typename Response>
std::expected<Response, generic_error> send_post_request(const http_client& client, const std::string& path,
                                                         const Request& req) noexcept {
    return encode_payload(req)
        .and_then([&](auto payload) { return client.post(path, payload); })
        .and_then([](auto res) { return decode_payload<Response>(res.body); });
}

class gitea_runner_service_client::gitea_api_header_source : public http_header_source {
public:
    void set_headers(std::function<auto(const std::string& name, const std::string& value)->void> cb) {
        if (!m_uuid.empty()) {
            cb("X-Runner-UUID", m_uuid);
        }

        if (!m_token.empty()) {
            cb("X-Runner-Token", m_token);
        }
    }

    void set_uuid(const std::string& uuid) { m_uuid = uuid; }
    void set_token(const std::string& token) { m_token = token; }

private:
    std::string m_uuid;
    std::string m_token;
};

gitea_runner_service_client::gitea_runner_service_client(const std::string& instance_url)
        : m_header_source{std::make_shared<gitea_api_header_source>()}, m_client{instance_url, m_header_source} {}

void gitea_runner_service_client::set_credentials(gitea_runner_credentials credentials) {
    m_header_source->set_uuid(credentials.uuid);
    m_header_source->set_token(credentials.token);
}

std::expected<::ping::v1::PingResponse, generic_error>
gitea_runner_service_client::ping(::ping::v1::PingRequest req) const noexcept {
    return send_post_request<::ping::v1::PingRequest, ::ping::v1::PingResponse>(m_client, "/ping.v1.PingService/Ping",
                                                                                req);
}

std::expected<::runner::v1::RegisterResponse, generic_error>
gitea_runner_service_client::register_(::runner::v1::RegisterRequest req) const noexcept {
    return send_post_request<::runner::v1::RegisterRequest, ::runner::v1::RegisterResponse>(
        m_client, "/runner.v1.RunnerService/Register", req);
}

std::expected<::runner::v1::DeclareResponse, generic_error>
gitea_runner_service_client::declare(::runner::v1::DeclareRequest req) const noexcept {
    return send_post_request<::runner::v1::DeclareRequest, ::runner::v1::DeclareResponse>(
        m_client, "/runner.v1.RunnerService/Declare", req);
}

std::expected<::runner::v1::FetchTaskResponse, generic_error>
gitea_runner_service_client::fetch_task(::runner::v1::FetchTaskRequest req) const noexcept {
    return send_post_request<::runner::v1::FetchTaskRequest, ::runner::v1::FetchTaskResponse>(
        m_client, "/runner.v1.RunnerService/FetchTask", req);
}

std::expected<::runner::v1::UpdateTaskResponse, generic_error>
gitea_runner_service_client::update_task(::runner::v1::UpdateTaskRequest req) const noexcept {
    return send_post_request<::runner::v1::UpdateTaskRequest, ::runner::v1::UpdateTaskResponse>(
        m_client, "/runner.v1.RunnerService/UpdateTask", req);
}

std::expected<::runner::v1::UpdateLogResponse, generic_error>
gitea_runner_service_client::update_log(::runner::v1::UpdateLogRequest req) const noexcept {
    return send_post_request<::runner::v1::UpdateLogRequest, ::runner::v1::UpdateLogResponse>(
        m_client, "/runner.v1.RunnerService/UpdateLog", req);
}

} // namespace ls_gitea_runner::gitea
