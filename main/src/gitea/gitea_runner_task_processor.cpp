#include "gitea_runner_task_processor.hpp"
#include "../machine/machine_manager_factory_selector.hpp"
#include "../protobuf_helper.hpp"
#include "gitea_runner_service_client.hpp"
#include "gitea_workflow.hpp"
#include "gitea_workflow_executor.hpp"

#include <boost/json.hpp>

namespace ls_gitea_runner::gitea {
namespace {
std::string make_unique_name_workspace_name_for_task(const ::runner::v1::Task& task) {
    const auto& fields{task.context().fields()};
    const auto& repository_url{
        fields.at(fields.contains("repository_url") ? "repository_url" : "repositoryUrl").string_value()};
    const auto& run_id_str{fields.at("run_id").string_value()};
    const auto& run_attempt_str{fields.at("run_attempt").string_value()};
    const auto hash{std::hash<std::string>{}(repository_url + run_id_str + run_attempt_str)};
    return std::format("{0:x}", hash);
}
} // namespace

class gitea_runner_task_processor::impl final {
public:
    impl(const gitea_runner_service_client& client, const config::runner_config& config)
            : m_client{client}, m_config{config} {}

    std::expected<void, generic_error> process(::runner::v1::Task task) noexcept {
        using namespace std::literals;
        try {

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

            const auto& runs_on_label{wf_get_label_from_job_yaml(*yaml_job)};
            if (!runs_on_label) {
                return std::unexpected{runs_on_label.error()};
            }

            const auto& machine_env{m_config.get().find_environment_by_label(*runs_on_label)};
            if (!machine_env) {
                return std::unexpected{
                    generic_error{std::format("No environment found for label: {}", *runs_on_label)}};
            }
            const auto& [machine_type, machine_config]{*machine_env};

            auto machine_manager_factory_res{machine_manager_factory_selector::get_factory(machine_type)};
            if (!machine_manager_factory_res) {
                return std::unexpected{machine_manager_factory_res.error()};
            }

            auto machine_manager_factory{std::move(*machine_manager_factory_res)};
            auto machine_manager{machine_manager_factory->create()};
            auto machine_res{machine_manager->spawn(machine_config.details_as_json)};

            if (!machine_res) {
                return std::unexpected{machine_res.error()};
            }

            auto machine{std::move(*machine_res)};
            if (!machine->wait_until_ready(60s)) {
                return std::unexpected{generic_error{
                    std::format("Timed out while waiting for machine to become ready: {}", machine->get_id())}};
            }

            const auto& runner_name{m_config.get().name};
            const auto& workspace_name{make_unique_name_workspace_name_for_task(task)};
            const std::string dir_separator{machine_config.os == "Windows" ? "\\" : "/"};
            const std::string workspace_dir{machine_config.workspaces_dir + dir_separator + workspace_name};

            auto modified_main_context{task.context()};
            if (auto* fields{modified_main_context.mutable_fields()}) {
                if (auto* value{fields->at("workspace").mutable_string_value()}) {
                    *value = workspace_dir;
                }
            }

            wf_run_contexts wf_contexts;
            try {
                wf_contexts.main = boost::json::serialize(protobuf::protobuf_to_json(modified_main_context));
                wf_contexts.vars = boost::json::serialize(protobuf::protobuf_to_json(task.vars()));
                wf_contexts.secrets = boost::json::serialize(protobuf::protobuf_to_json(task.secrets()));
                wf_contexts.runner = wf_create_runner_context(runner_name, machine_config);
                wf_contexts.matrix = wf_load_matrix_context_from_job_yaml(*yaml_job);
            } catch (const std::exception& ex) {
                return std::unexpected{
                    generic_error{std::format("Failed to build contexts for task #{}: {}", task.id(), ex.what())}};
            }

            const auto initial_env{wf_create_initial_env(wf_contexts)};
            if (!initial_env) {
                return std::unexpected{initial_env.error()};
            }

            const auto global_env{wf_load_and_derive_env_from_yaml(*workflow_payload, *initial_env, wf_contexts)};
            const auto job_env{wf_load_and_derive_env_from_yaml(*yaml_job, global_env, wf_contexts)};

            const auto wf_job{wf_load_job_with_name(*workflow_payload, job_name, wf_contexts)};
            if (!wf_job) {
                return std::unexpected{wf_job.error()};
            }

            gitea_workflow_executor executor{m_client.get(),     task,         *wf_job, job_env, wf_contexts,
                                             std::move(machine), workspace_dir};
            return executor.run();
        } catch (const std::exception& ex) {
            return std::unexpected{generic_error{std::format("Failed to process task #{}: {}", task.id(), ex.what())}};
        }
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
