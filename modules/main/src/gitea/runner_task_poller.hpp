#pragma once

#include "../config.hpp"
#include "runner_service_client.hpp"
#include "runner/v1/messages.pb.h"

#include <expected>
#include <functional>
#include <memory>

namespace ls_gitea_runner::gitea {

class GiteaRunnerTaskPoller final {
public:
    using RunCallbackFn = std::function<void(::runner::v1::Task)>;

    GiteaRunnerTaskPoller(const GiteaRunnerServiceClient& client, const config::RunnerConfig& config, RunCallbackFn cb);
    ~GiteaRunnerTaskPoller();

    GiteaRunnerTaskPoller(const GiteaRunnerTaskPoller&) = delete;
    GiteaRunnerTaskPoller& operator=(const GiteaRunnerTaskPoller&) = delete;

    void run();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner::gitea
