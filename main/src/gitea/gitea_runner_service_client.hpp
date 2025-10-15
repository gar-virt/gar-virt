#pragma once

#include "../error.hpp"
#include "../http.hpp"
#include "gitea_runner_credentials.hpp"
#include "ping/v1/messages.pb.h"
#include "runner/v1/messages.pb.h"

#include <expected>
#include <memory>

namespace ls_gitea_runner::gitea {

class gitea_runner_service_client final {
public:
    gitea_runner_service_client(const std::string& instance_url);

    void set_credentials(gitea_runner_credentials credentials);

    std::expected<::ping::v1::PingResponse, generic_error> ping(::ping::v1::PingRequest req) const noexcept;

    std::expected<::runner::v1::RegisterResponse, generic_error>
    register_(::runner::v1::RegisterRequest req) const noexcept;

    std::expected<::runner::v1::DeclareResponse, generic_error>
    declare(::runner::v1::DeclareRequest req) const noexcept;

    std::expected<::runner::v1::FetchTaskResponse, generic_error>
    fetch_task(::runner::v1::FetchTaskRequest req) const noexcept;

    std::expected<::runner::v1::UpdateTaskResponse, generic_error>
    update_task(::runner::v1::UpdateTaskRequest req) const noexcept;

    std::expected<::runner::v1::UpdateLogResponse, generic_error>
    update_log(::runner::v1::UpdateLogRequest req) const noexcept;

private:
    class gitea_api_header_source;

    std::shared_ptr<gitea_api_header_source> m_header_source;
    http_client m_client;
};

} // namespace ls_gitea_runner::gitea
