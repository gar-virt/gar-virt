#pragma once

#include "../error.hpp"
#include "../http.hpp"
#include "ping/v1/messages.pb.h"
#include "runner/v1/messages.pb.h"
#include "runner_credentials.hpp"

#include <expected>

namespace ls_gitea_runner::gitea {

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
    HttpClient m_client;
};

} // namespace ls_gitea_runner::gitea
