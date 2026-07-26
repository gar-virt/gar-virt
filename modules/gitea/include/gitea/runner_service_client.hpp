#pragma once

#include <gitea/runner_credentials.hpp>
#include <ping/v1/messages.pb.h>
#include <runner/v1/messages.pb.h>
#include <utility/error.hpp>
#include <utility/http.hpp>

#include <expected>

namespace gv::gitea {

template <typename T> std::expected<std::vector<std::byte>, Error> encode_payload(const T& msg);
template <typename T> std::expected<T, Error> decode_payload(const std::vector<std::byte>& payload);

class GiteaRunnerServiceClient final {
public:
    GiteaRunnerServiceClient(const std::string& instance_url);
    void set_credentials(GiteaRunnerCredentials credentials);
    std::expected<::ping::v1::PingResponse, Error> ping(const ::ping::v1::PingRequest& req) const;

    std::expected<::runner::v1::RegisterResponse, Error> register_(const ::runner::v1::RegisterRequest& req) const;

    std::expected<::runner::v1::DeclareResponse, Error> declare(const ::runner::v1::DeclareRequest& req) const;

    std::expected<::runner::v1::FetchTaskResponse, Error> fetch_task(const ::runner::v1::FetchTaskRequest& req) const;

    std::expected<::runner::v1::UpdateTaskResponse, Error>
    update_task(const ::runner::v1::UpdateTaskRequest& req) const;

    std::expected<::runner::v1::UpdateLogResponse, Error> update_log(const ::runner::v1::UpdateLogRequest& req) const;

private:
    GiteaRunnerCredentials m_credentials;
    utility::HttpClient m_client;
};

} // namespace gv::gitea
