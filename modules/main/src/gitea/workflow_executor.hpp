#pragma once

#include "../machine/machine.hpp"
#include "runner_service_client.hpp"
#include "workflow.hpp"
#include "runner/v1/messages.pb.h"

#include <expected>
#include <string>

namespace ls_gitea_runner::gitea {

class GiteaWorkflowExecutor final {
public:
    GiteaWorkflowExecutor(const GiteaRunnerServiceClient& client, ::runner::v1::Task task, const WfJob& job,
                          WfEnvVars job_env, const WfRunContexts& wf_contexts, std::unique_ptr<Machine> machine,
                          const std::string& working_dir);
    ~GiteaWorkflowExecutor();

    std::expected<void, GenericError> run();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner::gitea
