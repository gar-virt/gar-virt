#include "gitea_workflow_executor.hpp"
#include "../protobuf_helper.hpp"
#include "../scripting.hpp"
#include "gitea_runner_service_client.hpp"
#include "gitea_workflow.hpp"
#include "runner/v1/messages.pb.h"

#include <utility/env.hpp>
#include <utility/string.hpp>
#include <utility/temporary_file.hpp>

#include <expected>
#include <format>
#include <print>

namespace ls_gitea_runner::gitea {

struct step_execution_context {
    const wf_step* step{};
    ::runner::v1::StepState* state{};
    wf_run_contexts wf_contexts;

    bool is_ok() noexcept {
        using namespace ::runner::v1;
        return state && (state->result() == RESULT_SKIPPED || state->result() == RESULT_SUCCESS);
    }
};

inline bool is_execution_result_ok(::runner::v1::Result result) noexcept {
    using namespace ::runner::v1;
    return result == RESULT_SKIPPED || result == RESULT_SUCCESS;
}

inline std::string generate_shell_execution_intermediate_script(const std::string& shell_name,
                                                                const std::string& real_script_path) {
    static constexpr auto format_string{R"script(
set -e
shell=${{1}}
script_file=${{2}}
script_file=${{script_file//\\//}}
if [[ -z "${{shell}}" ]]; then
    if [[ "${{OSTYPE}}" =~ cygwin|msys ]]; then
        shell=pwsh
    else
        shell=bash
    fi
fi
case "${{shell}}" in
    pwsh)
        pwsh -command ". \"${{script_file}}\""
        ;;
    powershell)
        powershell -command ". \"${{script_file}}\""
        ;;
    cmd)
        "${{ComSpec}}" /D /E:ON /V:OFF /S /C "CALL "${{script_file}}""
        ;;
    sh)
        sh -e "${{script_file}}"
        ;;
    bash)
        bash -e "${{script_file}}"
        ;;
    python)
        python -e "${{script_file}}"
        ;;
    *)
        echo "Invalid shell: ${{shell}}"
        exit 1
        ;;
esac
)script"};
    const auto script{std::format(format_string, shell_name, real_script_path)};
    return script;
}

inline bool execute_shell_script(const wf_step_run& input, const wf_env_vars& env) {
    auto shell_name{input.shell.value_or("")};
    const auto script_str{input.script};
    fs::temporary_file real_script_file;
    {
        auto script_file{real_script_file.create_output_stream()};
        script_file->write(script_str.data(), script_str.size());
    }
    const auto real_script_path{utility::string_from_u8string(real_script_file.get_path().u8string())};
    const auto intermediate_script{generate_shell_execution_intermediate_script(shell_name, real_script_path)};
    fs::temporary_file intermeriate_script_file;
    {
        auto script_file{intermeriate_script_file.create_output_stream()};
        script_file->write(intermediate_script.data(), intermediate_script.size());
    }
    const auto intermeriate_script_path{utility::string_from_u8string(intermeriate_script_file.get_path().u8string())};
    const auto cmd{std::format("bash -e '{}' '{}' '{}'", intermeriate_script_path, shell_name, real_script_path)};

    // Back up environment variables
    std::unordered_map<std::string, std::optional<std::string>> original_env;
    for (const auto& [k, v] : *env) {
        original_env[k] = utility::getenv(k);
    }

    // Set environment variables
    for (const auto& [k, v] : *env) {
        utility::setenv(k, v);
    }

    // TODO: Check UTF-8 compatibility on Windows
    bool exec_ok{std::system(cmd.c_str()) == 0};

    // Restore environment variables
    for (const auto& [k, v] : original_env) {
        if (v) {
            utility::setenv(k, *v);
        } else {
            utility::unsetenv(k);
        }
    }

    return exec_ok;
}

inline bool execute_action(const wf_step_uses& input, const wf_env_vars& env) {
    std::println("TODO: execute_action(url: {})", input.url);
    return true;
}

inline ::runner::v1::Result task_result_from_step_states(const std::vector<step_execution_context>& steps) noexcept {
    using namespace ::runner::v1;
    for (const auto& step : steps) {
        switch (step.state->result()) {
        case RESULT_SKIPPED:
        case RESULT_SUCCESS:
            continue;
        case RESULT_CANCELLED:
            return RESULT_CANCELLED;
        default:
            return RESULT_FAILURE;
        }
    }
    return RESULT_SUCCESS;
}

inline ::runner::v1::Result execute_job_step(step_execution_context& ctx, const wf_env_vars& env) {
    auto* step{ctx.step};
    auto* step_state{ctx.state};
    bool ok{};
    if (step->run) {
        ok = execute_shell_script(*step->run, env);
    } else if (step->uses) {
        ok = execute_action(*step->uses, env);
    }
    return ok ? ::runner::v1::RESULT_SUCCESS : ::runner::v1::RESULT_FAILURE;
}

class gitea_workflow_executor::impl final {
public:
    impl(const gitea_runner_service_client& client, ::runner::v1::Task task, const wf_job& job, wf_env_vars job_env,
         const wf_run_contexts& wf_contexts)
            : m_client{client}, m_task{std::move(task)}, m_job{job}, m_job_env{std::move(job_env)},
              m_wf_contexts{wf_contexts} {}

    std::expected<void, generic_error> run() {
        const auto create_update_task_failure_error{
            [&] { return std::unexpected{generic_error{std::format("Failed to update #{}", m_task.id())}}; }};

        // Initial task state
        auto update_task_request{::runner::v1::UpdateTaskRequest{}};
        auto* task_state{update_task_request.mutable_state()};
        {
            task_state->set_id(m_task.id());
            task_state->set_result(::runner::v1::RESULT_UNSPECIFIED);

            auto update_task_response{m_client.get().update_task(update_task_request)};
            if (!update_task_response) {
                return create_update_task_failure_error();
            }
        }

        std::vector<step_execution_context> step_executions;

        // Set initial step state
        {
            const auto ts{protobuf::current_timestamp()};
            task_state->mutable_started_at()->set_seconds(ts.seconds());
            task_state->mutable_started_at()->set_nanos(ts.nanos());

            int i{};
            for (const auto& wf_step : m_job.get().steps) {
                auto* step_state{task_state->add_steps()};
                step_state->set_id(i);
                step_state->set_result(::runner::v1::RESULT_UNSPECIFIED);
                step_state->set_log_index(0);
                step_state->set_log_length(0);
                step_executions.push_back({.step = &wf_step, .state = step_state, .wf_contexts = m_wf_contexts.get()});
                ++i;
            }

            auto update_task_response{m_client.get().update_task(update_task_request)};
            if (!update_task_response) {
                return create_update_task_failure_error();
            }
        }

        // Execute steps while updating status
        {
            for (auto& step_execution : step_executions) {
                auto* step{step_execution.step};
                auto* step_state{step_execution.state};
                const auto step_env{wf_load_and_derive_env_from_yaml(step->yaml, m_job_env, m_wf_contexts.get())};
                scripting::expression_evaluator evaluator{step_execution.wf_contexts.to_list()};
                // Started
                {
                    const auto ts{protobuf::current_timestamp()};
                    step_state->mutable_started_at()->set_seconds(ts.seconds());
                    step_state->mutable_started_at()->set_nanos(ts.nanos());

                    auto update_task_response{m_client.get().update_task(update_task_request)};
                    if (!update_task_response) {
                        return create_update_task_failure_error();
                    }
                }
                auto step_result{::runner::v1::RESULT_UNSPECIFIED};
                // "Work"
                if (step->condition) {
                    const auto eval_result{evaluator.eval_true(*step->condition)};
                    if (!eval_result) {
                        step_result = ::runner::v1::RESULT_FAILURE;
                    } else if (!*eval_result) {
                        step_result = ::runner::v1::RESULT_SKIPPED;
                    }
                }
                if (step_result == ::runner::v1::RESULT_UNSPECIFIED) {
                    step_result = execute_job_step(step_execution, step_env);
                }
                // Completed
                {
                    const auto ts{protobuf::current_timestamp()};
                    step_state->mutable_stopped_at()->set_seconds(ts.seconds());
                    step_state->mutable_stopped_at()->set_nanos(ts.nanos());
                    step_state->set_result(step_result);

                    auto update_task_response{m_client.get().update_task(update_task_request)};
                    if (!update_task_response) {
                        return create_update_task_failure_error();
                    }
                }
                if (!is_execution_result_ok(step_result)) {
                    break;
                }
            }
        }

        // Completion
        {
            const auto task_result{task_result_from_step_states(step_executions)};
            const auto ts{protobuf::current_timestamp()};
            task_state->mutable_stopped_at()->set_seconds(ts.seconds());
            task_state->mutable_stopped_at()->set_nanos(ts.nanos());
            task_state->set_result(task_result);

            auto update_task_response{m_client.get().update_task(update_task_request)};
            if (!update_task_response) {
                return create_update_task_failure_error();
            }
        }

        return {};
    }

private:
    std::reference_wrapper<const gitea_runner_service_client> m_client;
    ::runner::v1::Task m_task;
    std::reference_wrapper<const wf_job> m_job;
    wf_env_vars m_job_env;
    std::reference_wrapper<const wf_run_contexts> m_wf_contexts;
};

gitea_workflow_executor::gitea_workflow_executor(const gitea_runner_service_client& client, ::runner::v1::Task task,
                                                 const wf_job& job, wf_env_vars job_env,
                                                 const wf_run_contexts& wf_contexts)
        : m_impl{std::make_unique<impl>(client, std::move(task), job, std::move(job_env), wf_contexts)} {}

gitea_workflow_executor::~gitea_workflow_executor() {}

std::expected<void, generic_error> gitea_workflow_executor::run() { return m_impl->run(); }

} // namespace ls_gitea_runner::gitea
