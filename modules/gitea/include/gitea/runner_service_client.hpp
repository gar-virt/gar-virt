#pragma once

#include <gitea/runner_credentials.hpp>
#include <ping/v1/messages.pb.h>
#include <runner/v1/messages.pb.h>
#include <utility/error.hpp>
#include <utility/http.hpp>

#include <expected>

namespace ls_gitea_runner::gitea {

template <typename T> std::expected<std::vector<std::byte>, GenericError> encode_payload(const T& msg) noexcept;
template <typename T> std::expected<T, GenericError> decode_payload(const std::vector<std::byte>& payload) noexcept;

class GiteaRunnerServiceClient final {
public:
    GiteaRunnerServiceClient(const std::string& instance_url);

    void set_credentials(GiteaRunnerCredentials credentials);

    std::expected<::ping::v1::PingResponse, GenericError> ping(::ping::v1::PingRequest req) const noexcept;

    std::expected<::runner::v1::RegisterResponse, GenericError>
    register_(::runner::v1::RegisterRequest req) const noexcept;

    std::expected<::runner::v1::DeclareResponse, GenericError> declare(::runner::v1::DeclareRequest req) const noexcept;

    std::expected<::runner::v1::FetchTaskResponse, GenericError>
    fetch_task(::runner::v1::FetchTaskRequest req) const noexcept;

    std::expected<::runner::v1::UpdateTaskResponse, GenericError>
    update_task(::runner::v1::UpdateTaskRequest req) const noexcept;

    std::expected<::runner::v1::UpdateLogResponse, GenericError>
    update_log(::runner::v1::UpdateLogRequest req) const noexcept;

private:
    GiteaRunnerCredentials m_credentials;
    utility::HttpClient m_client;
};

} // namespace ls_gitea_runner::gitea
