#pragma once

#include "gitea_runner_service_client.hpp"
#include "gitea_workflow.hpp"
#include "runner/v1/messages.pb.h"

#include <expected>

namespace ls_gitea_runner::gitea {

class gitea_workflow_executor final {
public:
    gitea_workflow_executor(const gitea_runner_service_client& client, ::runner::v1::Task task, const wf_job& job,
                            wf_env_vars job_env, const wf_run_contexts& wf_contexts);
    ~gitea_workflow_executor();

    std::expected<void, generic_error> run();

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace ls_gitea_runner::gitea
