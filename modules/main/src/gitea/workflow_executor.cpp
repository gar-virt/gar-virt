#include "workflow_executor.hpp"
#include "../protobuf_helper.hpp"
#include "../scripting.hpp"
#include "log_reporter.hpp"
#include "runner_service_client.hpp"
#include "workflow.hpp"
#include "runner/v1/messages.pb.h"

#include <utility/defer.hpp>
#include <utility/env.hpp>
#include <utility/string.hpp>
#include <utility/temporary_file.hpp>
#include <utility/uuid.hpp>

#include <atomic>
#include <chrono>
#include <expected>
#include <format>
#include <print>
#include <regex>
#include <string_view>
#include <thread>
#include <vector>

namespace ls_gitea_runner::gitea {
namespace {

struct StepExecutionContext {
    const WfStep* step{};
    ::runner::v1::StepState* state{};
    WfRunContexts wf_contexts;

    bool is_ok() noexcept {
        using namespace ::runner::v1;
        return state && (state->result() == RESULT_SKIPPED || state->result() == RESULT_SUCCESS);
    }
};

bool is_execution_result_ok(::runner::v1::Result result) noexcept {
    using namespace ::runner::v1;
    return result == RESULT_SKIPPED || result == RESULT_SUCCESS;
}

std::string escape_shell_script_string(const std::string_view arg, bool add_quotes = true) {
    std::string result;
    result.reserve(arg.size() + (add_quotes ? 2 : 0)); // Reserve space for input and quotes
    if (add_quotes) {
        result += '"';
    }
    for (char c : arg) {
        switch (c) {
        case '"':
        case '$':
        case '\\':
            result += '\\';
            result += c;
            break;
        case '\n':
            result += "\\n";
            break;
        default:
            result += c;
            break;
        }
    }
    if (add_quotes) {
        result += '"';
    }
    return result;
}

std::string make_env_script(const WfEnvVars& env) {
    std::string env_script;
    if (env) {
        for (const auto& [k, v] : *env) {
            static std::regex pattern{R"([^a-zA-Z0-9_])"};
            const auto sanetized_key{std::regex_replace(k, pattern, "_")};
            const auto escaped_value{escape_shell_script_string(v)};
            env_script += std::format("export {}={}\n", sanetized_key, escaped_value);
        }
    }
    return env_script;
}

std::string generate_shell_execution_intermediate_script(const std::string& shell_name,
                                                         const std::string& real_script_path,
                                                         const std::string& working_dir, const WfEnvVars& env) {
    const auto env_script{make_env_script(env)};
    static constexpr auto format_string{R"script(
set -e
shell={0}
script_file={1}
script_file=${{script_file//\\//}}
working_dir={2}
if [[ -z "${{shell}}" ]]; then
    if [[ "${{OSTYPE}}" =~ cygwin|msys ]]; then
        shell=pwsh
    else
        shell=bash
    fi
fi
mkdir -p "${{working_dir}}"
cd "${{working_dir}}"
{3}
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
    const auto script{std::format(format_string, escape_shell_script_string(shell_name),
                                  escape_shell_script_string(real_script_path), escape_shell_script_string(working_dir),
                                  env_script)};
    return script;
}

bool execute_shell_script(const WfStepRun& input, const WfEnvVars& env, std::unique_ptr<Machine>& machine,
                          const std::string& working_dir, LogReporter& reporter,
                          std::function<std::int64_t(const char*, int)> stdout_reader) {
    // TODO: don't store scripts locally, even if only temporarily
    const auto script_str{input.script};
    fs::TemporaryFile real_script_file;
    {
        auto script_file{real_script_file.create_output_stream()};
        script_file->write(script_str.data(), script_str.size());
    }
    auto shell_name{input.shell.value_or("")};
    const auto local_real_script_path{real_script_file.get_path()};
    const auto local_real_script_path_str{utility::string_from_u8string(local_real_script_path.u8string())};
    const auto remote_real_script_path_str{std::format("/tmp/script_{}.sh", utility::uuid())};
    const auto local_intermediate_script{
        generate_shell_execution_intermediate_script(shell_name, remote_real_script_path_str, working_dir, env)};
    fs::TemporaryFile local_intermediate_script_file;
    {
        auto script_file{local_intermediate_script_file.create_output_stream()};
        script_file->write(local_intermediate_script.data(), local_intermediate_script.size());
    }
    const auto intermediate_script_path{local_intermediate_script_file.get_path()};
    const auto intermediate_script_path_str{utility::string_from_u8string(intermediate_script_path.u8string())};
    const auto remote_intermediate_script_path_str{std::format("/tmp/script_{}.sh", utility::uuid())};
    utility::SpawnOptions spawn_options{.stdout_reader = std::move(stdout_reader)};
    const auto exec_result{
        machine->copy_file_into(local_real_script_path, remote_real_script_path_str)
            .and_then(
                [&] { return machine->copy_file_into(intermediate_script_path, remote_intermediate_script_path_str); })
            .and_then([&] {
                return machine->shell_exec({"/usr/bin/bash", "-e", remote_intermediate_script_path_str}, spawn_options);
            })};
    return exec_result && *exec_result == 0;
#if 0
    // This is for local runs
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
#endif
}

struct ActionUrl {
    std::string url;
    std::string version;
    bool is_local{};

    static ActionUrl parse(const std::string_view input,
                           const std::string_view default_base_url = "https://github.com") {
        if (input.starts_with("./")) {
            return ActionUrl{.url = std::string{input}, .is_local = true};
        }
        auto parts{utility::string_split(input, '@')};
        std::string full_url;
        if (!parts.at(0).contains("://")) {
            full_url += default_base_url;
        }
        if (!full_url.ends_with('/')) {
            full_url += '/';
        }
        full_url += parts.at(0);
        return ActionUrl{.url = std::move(full_url),
                         .version = parts.size() > 1 ? std::move(parts.at(1)) : std::string{},
                         .is_local = false};
    }
};

bool execute_js_action(const WfEnvVars& env, std::unique_ptr<Machine>& machine, const std::string& action_dir,
                       LogReporter& reporter, std::function<std::int64_t(const char*, int)> stdout_reader) {
    using namespace std::literals;
    auto env_copy{std::make_shared<WfEnvVars::element_type>(*env)};
    // Token should be set automatically
    env_copy->emplace("INPUT_TOKEN", (*env_copy)["GITHUB_TOKEN"]);
    const auto env_script{make_env_script(env_copy)};
    const auto script{std::format(R"script(
set -e
action_dir={0}
cd "${{action_dir}}"
{1}
node dist/index.js
)script",
                                  escape_shell_script_string(action_dir), env_script)};
    std::int64_t amount_copied{};
    utility::SpawnOptions spawn_options{
        .stdin_writer = [&](char* buffer, int buffer_length) -> std::int64_t {
            if (buffer_length < 1 || amount_copied >= script.size()) {
                return 0;
            }
            const auto amount{std::min<std::int64_t>(
                script.size() - static_cast<std::size_t>(amount_copied),
                std::min<std::int64_t>(buffer_length, static_cast<std::int64_t>(script.size())))};
            std::memcpy(buffer, script.data() + amount_copied, static_cast<std::size_t>(amount));
            amount_copied += amount;
            return amount;
        },
        .stdout_reader = std::move(stdout_reader)};
    const auto exec_result{machine->shell_exec({"bash"}, spawn_options)};
    return exec_result && *exec_result == 0;
}

bool fetch_action(const ActionUrl& action_url, std::unique_ptr<Machine>& machine, const std::string& action_dir,
                  LogReporter& reporter, std::function<std::int64_t(const char*, int)> stdout_reader) {
    using namespace std::literals;
    const auto script{std::format(R"script(
set -e
action_dir={0}
action_url={1}
action_version={2}
mkdir -p "${{action_dir}}"
cd "${{action_dir}}"
git init
git remote add origin "${{action_url}}"
git fetch --depth=1 origin tag "${{action_version}}"
git switch --detach "${{action_version}}"
)script",
                                  escape_shell_script_string(action_dir), escape_shell_script_string(action_url.url),
                                  escape_shell_script_string(action_url.version))};
    std::int64_t amount_copied{};
    utility::SpawnOptions spawn_options{
        .stdin_writer = [&](char* buffer, int buffer_length) -> std::int64_t {
            if (buffer_length < 1 || amount_copied >= script.size()) {
                return 0;
            }
            const auto amount{std::min<std::int64_t>(
                script.size() - static_cast<std::size_t>(amount_copied),
                std::min<std::int64_t>(buffer_length, static_cast<std::int64_t>(script.size())))};
            std::memcpy(buffer, script.data() + amount_copied, static_cast<std::size_t>(amount));
            amount_copied += amount;
            return amount;
        },
        .stdout_reader = std::move(stdout_reader)};
    const auto exec_result{machine->shell_exec({"bash"}, spawn_options)};
    return exec_result && *exec_result == 0;
}

bool execute_action(const WfStepUses& input, const WfEnvVars& env, std::unique_ptr<Machine>& machine,
                    const std::string& working_dir, LogReporter& reporter,
                    std::function<std::int64_t(const char*, int)> stdout_reader) {
    using namespace std::literals;
    const auto action_url{ActionUrl::parse(input.url)};
    std::string action_dir;
    const std::string dir_separator{machine->info().os == "Windows" ? "\\" : "/"};
    if (!action_url.is_local) {
        // Need to fetch it
        const auto url_hash{std::to_string(std::hash<std::string>{}(action_url.url))};
        action_dir = utility::string_join(dir_separator, working_dir, "_actions", url_hash);
        if (!fetch_action(action_url, machine, action_dir, reporter, stdout_reader)) {
            return false;
        }
    } else {
        action_dir = action_url.url;
    }
    return execute_js_action(env, machine, action_dir, reporter, std::move(stdout_reader));
}

::runner::v1::Result task_result_from_step_states(const std::vector<StepExecutionContext>& steps) noexcept {
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

::runner::v1::Result execute_job_step(StepExecutionContext& ctx, const WfEnvVars& env,
                                      std::unique_ptr<Machine>& machine, const std::string& working_dir,
                                      LogReporter& reporter) {
    std::string buffer;
    std::function<std::int64_t(const char*, int)> stdout_reader{[&](const char* data, int length) {
        buffer.append(data, length);
        auto [lines, remainder]{utility::string_split_with_remainder(buffer, '\n')};
        for (auto& line : lines) {
            reporter.add(std::string{utility::string_trim_right(line, {'\r'})});
        }
        buffer = std::move(remainder);
        return length;
    }};
    auto deferred_last_report{utility::defer([&] {
        if (!buffer.empty()) {
            reporter.add(std::move(buffer));
        }
    })};
    auto* step{ctx.step};
    auto* step_state{ctx.state};
    bool ok{};
    if (step->run) {
        ok = execute_shell_script(*step->run, env, machine, working_dir, reporter, stdout_reader);
    } else if (step->uses) {
        ok = execute_action(*step->uses, env, machine, working_dir, reporter, stdout_reader);
    }
    return ok ? ::runner::v1::RESULT_SUCCESS : ::runner::v1::RESULT_FAILURE;
}

} // namespace

class GiteaWorkflowExecutor::Impl final {
public:
    Impl(const GiteaRunnerServiceClient& client, ::runner::v1::Task task, const WfJob& job, WfEnvVars job_env,
         const WfRunContexts& wf_contexts, std::unique_ptr<Machine> machine, const std::string& working_dir)
            : m_client{client}, m_task{std::move(task)}, m_job{job}, m_job_env{std::move(job_env)},
              m_wf_contexts{wf_contexts}, m_machine{std::move(machine)}, m_working_dir{working_dir} {}

    std::expected<void, GenericError> run() {
        const auto create_update_task_failure_error{
            [&] { return std::unexpected{GenericError{std::format("Failed to update task #{}", m_task.id())}}; }};

        GiteaLogReporter reporter{m_client.get(), m_task};
        reporter.add(std::format("Starting task #{}", m_task.id()));

        // Initial task state
        ::runner::v1::UpdateTaskRequest update_task_request;
        auto* task_state{update_task_request.mutable_state()};
        {
            task_state->set_id(m_task.id());
            task_state->set_result(::runner::v1::RESULT_UNSPECIFIED);

            auto update_task_response{m_client.get().update_task(update_task_request)};
            if (!update_task_response) {
                return create_update_task_failure_error();
            }
        }

        std::vector<StepExecutionContext> step_executions;

        // Set initial step state
        {
            const auto base_index{reporter.head()};
            const auto ts{protobuf::current_timestamp()};
            task_state->mutable_started_at()->set_seconds(ts.seconds());
            task_state->mutable_started_at()->set_nanos(ts.nanos());

            int i{};
            for (const auto& wf_step : m_job.get().steps) {
                auto* step_state{task_state->add_steps()};
                step_state->set_id(i);
                step_state->set_result(::runner::v1::RESULT_UNSPECIFIED);
                step_state->set_log_index(base_index);
                step_state->set_log_length(reporter.head() - base_index);
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
                const auto base_index{reporter.head()};
                auto* step{step_execution.step};
                auto* step_state{step_execution.state};
                const auto step_env{wf_load_and_derive_env_from_yaml(step->yaml, m_job_env, m_wf_contexts.get())};
                scripting::ExpressionEvaluator evaluator{step_execution.wf_contexts.to_list()};
                // Started
                {
                    const auto ts{protobuf::current_timestamp()};
                    step_state->mutable_started_at()->set_seconds(ts.seconds());
                    step_state->mutable_started_at()->set_nanos(ts.nanos());
                    step_state->set_log_index(base_index);
                    step_state->set_log_length(reporter.head() - base_index);

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
                    std::atomic_bool done{};
                    std::jthread report_thread{[&] {
                        using namespace std::literals;
                        auto last_time{std::chrono::steady_clock::now()};
                        while (!done) {
                            const auto diff_time{std::chrono::steady_clock::now() - last_time};
                            if (diff_time >= 2s) {
                                // TODO: handle errors
                                reporter.flush();
                                step_state->set_log_length(reporter.head() - base_index);
                                std::ignore = m_client.get().update_task(update_task_request);
                                last_time = std::chrono::steady_clock::now();
                            }
                            std::this_thread::sleep_for(100ms);
                        }
                    }};
                    // TODO: timeout
                    step_result = execute_job_step(step_execution, step_env, m_machine, m_working_dir, reporter);
                    done = true;
                    report_thread.join();
                }
                // Completed
                {
                    const auto ts{protobuf::current_timestamp()};
                    step_state->mutable_stopped_at()->set_seconds(ts.seconds());
                    step_state->mutable_stopped_at()->set_nanos(ts.nanos());
                    step_state->set_result(step_result);
                    step_state->set_log_index(base_index);
                    step_state->set_log_length(reporter.head() - base_index);

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
            reporter.add(std::format("Completed task #{}", m_task.id()));
            reporter.close();

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
    std::reference_wrapper<const GiteaRunnerServiceClient> m_client;
    ::runner::v1::Task m_task;
    std::reference_wrapper<const WfJob> m_job;
    WfEnvVars m_job_env;
    std::reference_wrapper<const WfRunContexts> m_wf_contexts;
    std::unique_ptr<Machine> m_machine;
    std::string m_working_dir;
};

GiteaWorkflowExecutor::GiteaWorkflowExecutor(const GiteaRunnerServiceClient& client, ::runner::v1::Task task,
                                             const WfJob& job, WfEnvVars job_env, const WfRunContexts& wf_contexts,
                                             std::unique_ptr<Machine> machine, const std::string& working_dir)
        : m_impl{std::make_unique<Impl>(client, std::move(task), job, std::move(job_env), wf_contexts,
                                        std::move(machine), working_dir)} {}

GiteaWorkflowExecutor::~GiteaWorkflowExecutor() {}

std::expected<void, GenericError> GiteaWorkflowExecutor::run() { return m_impl->run(); }

} // namespace ls_gitea_runner::gitea
