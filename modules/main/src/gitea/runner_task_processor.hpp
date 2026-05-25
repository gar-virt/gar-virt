#pragma once

#include "../config.hpp"
#include "../error.hpp"
#include "runner_service_client.hpp"
#include "runner/v1/messages.pb.h"

#include <expected>
#include <memory>

namespace ls_gitea_runner::gitea {

class GiteaRunnerTaskProcessor final {
public:
    GiteaRunnerTaskProcessor(const GiteaRunnerServiceClient& client, const config::RunnerConfig& config);
    ~GiteaRunnerTaskProcessor();
    std::expected<void, GenericError> process(::runner::v1::Task task) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner::gitea
