#include "gitea_runner_task_processor.hpp"
#include "../protobuf_helper.hpp"
#include "gitea_runner_service_client.hpp"
#include "gitea_workflow.hpp"
#include "gitea_workflow_executor.hpp"

#include <boost/json.hpp>

namespace ls_gitea_runner::gitea {

class gitea_runner_task_processor::impl final {
public:
    impl(const gitea_runner_service_client& client, const config::runner_config& config)
            : m_client{client}, m_config{config} {}

    std::expected<void, generic_error> process(::runner::v1::Task task) noexcept {
        using namespace std::literals;
        const auto& task_context{task.context()};
        const auto& job_name{task_context.fields().at("job").string_value()};

        auto workflow_payload{wf_load_yaml(task.workflow_payload())};
        if (!workflow_payload) {
            return std::unexpected{workflow_payload.error()};
        }

        const auto yaml_job{wf_find_job_with_name_in_yaml(*workflow_payload, job_name)};
        if (!yaml_job) {
            return std::unexpected{yaml_job.error()};
        }

        wf_run_contexts wf_contexts;
        try {
            wf_contexts.main = boost::json::serialize(protobuf::protobuf_to_json(task.context()));
            wf_contexts.vars = boost::json::serialize(protobuf::protobuf_to_json(task.vars()));
            wf_contexts.secrets = boost::json::serialize(protobuf::protobuf_to_json(task.secrets()));
            wf_contexts.runner = wf_create_runner_context();
            wf_contexts.matrix = wf_load_matrix_context_from_job_yaml(*yaml_job);
        } catch (const std::exception&) {
            return std::unexpected{generic_error{std::format("Failed to build contexts for task #{}", task.id())}};
        }

        const auto global_env{wf_load_and_derive_env_from_yaml(*workflow_payload, {}, wf_contexts)};
        const auto job_env{wf_load_and_derive_env_from_yaml(*yaml_job, global_env, wf_contexts)};

        const auto wf_job{wf_load_job_with_name(*workflow_payload, job_name, wf_contexts)};
        if (!wf_job) {
            return std::unexpected{wf_job.error()};
        }

        gitea_workflow_executor executor{m_client.get(), task, *wf_job, job_env, wf_contexts};
        return executor.run();
    }

private:
    std::reference_wrapper<const gitea_runner_service_client> m_client;
    std::reference_wrapper<const config::runner_config> m_config;
};

gitea_runner_task_processor::gitea_runner_task_processor(const gitea_runner_service_client& client,
                                                         const config::runner_config& config)
        : m_impl{std::make_unique<impl>(client, config)} {}

gitea_runner_task_processor::~gitea_runner_task_processor() {}

std::expected<void, generic_error> gitea_runner_task_processor::process(::runner::v1::Task task) noexcept {
    return m_impl->process(task);
}

} // namespace ls_gitea_runner::gitea
