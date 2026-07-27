#pragma once

#include <gitea/runner_credentials.hpp>
#include <ping/v1/messages.pb.h>
#include <runner/v1/messages.pb.h>
#include <utility/http.hpp>
#include <utility/result.hpp>

#include <expected>

namespace gv::gitea {

template <typename T> Result<std::vector<std::byte>> encode_payload(const T& msg);
template <typename T> Result<T> decode_payload(const std::vector<std::byte>& payload);

class GiteaRunnerServiceClient final {
public:
    GiteaRunnerServiceClient(const std::string& instance_url);
    void set_credentials(GiteaRunnerCredentials credentials);
    Result<::ping::v1::PingResponse> ping(const ::ping::v1::PingRequest& req) const;

    Result<::runner::v1::RegisterResponse> register_(const ::runner::v1::RegisterRequest& req) const;

    Result<::runner::v1::DeclareResponse> declare(const ::runner::v1::DeclareRequest& req) const;

    Result<::runner::v1::FetchTaskResponse> fetch_task(const ::runner::v1::FetchTaskRequest& req) const;

    Result<::runner::v1::UpdateTaskResponse> update_task(const ::runner::v1::UpdateTaskRequest& req) const;

    Result<::runner::v1::UpdateLogResponse> update_log(const ::runner::v1::UpdateLogRequest& req) const;

private:
    GiteaRunnerCredentials m_credentials;
    utility::HttpClient m_client;
};

} // namespace gv::gitea
