#include "gitea_workflow.hpp"

#include "../scripting.hpp"

#include <utility/string.hpp>

#include <boost/json.hpp>
#include <yaml-cpp/yaml.h>

#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ls_gitea_runner::gitea {

std::vector<std::pair<std::string, std::string>> wf_run_contexts::to_list() const {
    std::vector<std::pair<std::string, std::string>> result;
    result.emplace_back("gitea", main);
    result.emplace_back("github", "gitea");
    result.emplace_back("vars", vars);
    result.emplace_back("secrets", secrets);
    result.emplace_back("runner", runner);
    result.emplace_back("matrix", matrix);
    return result;
}

std::vector<wf_step> wf_load_steps(const YAML::Node& yaml_steps, const wf_run_contexts& contexts) {
    const auto sub{scripting::apply_string_substitutions};
    const auto& contexts_list{contexts.to_list()};
    std::vector<wf_step> steps;
    for (auto& yaml_step : yaml_steps) {
        steps.push_back([&] {
            wf_step s;
            if (auto& condition{yaml_step["if"]}) {
                s.condition = sub(condition.as<std::string>(), contexts_list);
            }
            if (auto& name{yaml_step["name"]}) {
                s.name = sub(name.as<std::string>(), contexts_list);
            }
            auto& yaml_workdir{yaml_step["working-directory"]};
            auto workdir{yaml_workdir ? std::make_optional(
                                            std::filesystem::u8path(sub(yaml_workdir.as<std::string>(), contexts_list)))
                                      : std::nullopt};
            if (auto& run{yaml_step["run"]}) {
                auto& yaml_shell{yaml_step["shell"]};
                s.run = std::make_optional(wf_step_run{
                    .shell = yaml_shell ? std::make_optional(sub(yaml_shell.as<std::string>(), contexts_list))
                                        : std::nullopt,
                    .script = sub(run.as<std::string>(), contexts_list),
                    .working_directory = workdir,
                });
            }
            // Does "uses" also use working directory?
            if (auto& uses{yaml_step["uses"]}) {
                s.uses = std::make_optional(wf_step_uses{.url = sub(uses.as<std::string>(), contexts_list)});
            }
            if (!s.run && !s.uses) {
                throw generic_error{"Missing run/uses in workflow step"};
            }
            s.yaml = yaml_step.as<YAML::Node>();
            return s;
        }());
    }
    return steps;
}

std::expected<YAML::Node, generic_error> wf_load_yaml(const std::string& yaml_str) noexcept {
    try {
        return YAML::Load(yaml_str);
    } catch (const std::exception&) {
        return std::unexpected{generic_error{"Failed to parse workflow YAML"}};
    }
}

std::expected<YAML::Node, generic_error> wf_find_job_with_name_in_yaml(const YAML::Node& yaml,
                                                                       const std::string& name) noexcept {
    try {
        const auto& yaml_jobs{yaml["jobs"]};
        if (!yaml_jobs) {
            return std::unexpected{generic_error{"Missing jobs in workflow YAML"}};
        }
        const auto& yaml_job{yaml_jobs[name]};
        if (!yaml_job) {
            return std::unexpected{generic_error{std::format("Missing job named \"{}\" in workflow YAML", name)}};
        }
        return yaml_job;
    } catch (const std::exception& ex) {
        return std::unexpected{
            generic_error{std::format("Error while looking for job in workflow YAML: {}", ex.what())}};
    }
}

std::expected<wf_job, generic_error> wf_load_job_with_name(const YAML::Node& yaml, const std::string& name,
                                                           const wf_run_contexts& contexts) noexcept {
    try {
        const auto& yaml_job{wf_find_job_with_name_in_yaml(yaml, name)};
        if (!yaml_job) {
            return std::unexpected{generic_error{std::format("Couldn't find job named \"{}\" in workflow YAML", name)}};
        }
        const auto& yaml_steps{(*yaml_job)["steps"]};
        wf_job job{.steps = wf_load_steps(yaml_steps, contexts)};
        return job;
    } catch (const std::exception& ex) {
        return std::unexpected{
            generic_error{std::format("Error while loading job named \"{}\" in workflow YAML: {}", name, ex.what())}};
    }
}

std::expected<std::string, generic_error> wf_get_label_from_job_yaml(const YAML::Node& yaml) noexcept {
    const auto& runs_on{yaml["runs-on"]};
    if (!runs_on) {
        return std::unexpected{generic_error{"Couldn't find runs-on key in YAML workflow job"}};
    }
    return runs_on.as<std::string>();
}

using wf_env_vars = std::shared_ptr<std::unordered_map<std::string, std::string>>;

std::string wf_load_matrix_context_from_job_yaml(const YAML::Node& yaml) {
    const auto& yaml_strategy{yaml["strategy"]};
    if (!yaml_strategy) {
        return "{}";
    }
    const auto& yaml_matrix{yaml_strategy["matrix"]};
    if (!yaml_matrix) {
        return "{}";
    }
    if (!yaml_matrix.IsMap()) {
        return "{}";
    }
    boost::json::object json;
    for (auto& entry : yaml_matrix) {
        if (!entry.second.IsSequence()) {
            continue;
        }
        // TODO: better error handling
        try {
            const auto& key{entry.first.as<std::string>()};
            const auto& values{entry.second.as<std::vector<std::string>>()};
            json[key] = values.at(0);
        } catch (const std::exception&) {
            // Ignore
        }
    }
    return boost::json::serialize(json);
}

std::string wf_create_runner_context(const std::string& name, const config::runner_environment_config& config) {
    // TODO: tool_cache, debug, environment
    boost::json::object j = {
        {"name", name},
        {"os", config.os},
        {"arch", config.arch},
        {"temp", config.temp_dir},
    };
    return boost::json::serialize(j);
}

wf_env_vars wf_load_and_derive_env_from_yaml(const YAML::Node& yaml, wf_env_vars env, const wf_run_contexts& contexts) {
    if (!env) {
        env = std::make_shared<wf_env_vars::element_type>();
    }
    if (!yaml) {
        return env;
    }
    const auto& yaml_env{yaml["env"]};
    if (!yaml_env || !yaml_env.IsMap()) {
        return env;
    }
    const auto sub{scripting::apply_string_substitutions};
    const auto& contexts_list{contexts.to_list()};
    auto env_copy{std::make_shared<wf_env_vars::element_type>(*env)};
    for (auto& entry : yaml_env) {
        if (!entry.second.IsScalar()) {
            continue;
        }
        // TODO: may need conversion to string or error handling
        const auto& key{entry.first.as<std::string>()};
        auto value{sub(entry.second.as<std::string>(), contexts_list)};
        (*env_copy)[key] = std::move(value);
    }
    return env_copy;
}

std::expected<wf_env_vars, generic_error> wf_create_initial_env(const wf_run_contexts& contexts) {
    try {
        const auto main_ctx{boost::json::parse(contexts.main)};
        const auto runner_ctx{boost::json::parse(contexts.runner)};
        auto env_ptr{std::make_shared<wf_env_vars::element_type>()};
        auto& env{*env_ptr};
        env = {
            {"CI", "true"},
            {"GITHUB_ACTION", std::string{main_ctx.at("action").as_string()}},
            {"GITHUB_ACTION_PATH", std::string{main_ctx.at("action_path").as_string()}},
            {"GITHUB_ACTION_REPOSITORY", std::string{main_ctx.at("action_repository").as_string()}},
            //{"GITHUB_ACTIONS", ""},
            {"GITHUB_ACTOR", std::string{main_ctx.at("actor").as_string()}},
            //{"GITHUB_ACTOR_ID", ""},
            {"GITHUB_API_URL", std::string{main_ctx.at("api_url").as_string()}},
            {"GITHUB_BASE_REF", std::string{main_ctx.at("base_ref").as_string()}},
            {"GITHUB_ENV", std::string{main_ctx.at("env").as_string()}},
            {"GITHUB_EVENT_NAME", std::string{main_ctx.at("event_name").as_string()}},
            {"GITHUB_EVENT_PATH", std::string{main_ctx.at("event_path").as_string()}},
            {"GITHUB_GRAPHQL_URL", std::string{main_ctx.at("graphql_url").as_string()}},
            {"GITHUB_HEAD_REF", std::string{main_ctx.at("head_ref").as_string()}},
            {"GITHUB_JOB", std::string{main_ctx.at("job").as_string()}},
            //{"GITHUB_OUTPUT", ""},
            {"GITHUB_PATH", std::string{main_ctx.at("path").as_string()}},
            {"GITHUB_REF", std::string{main_ctx.at("ref").as_string()}},
            {"GITHUB_REF_NAME", std::string{main_ctx.at("ref_name").as_string()}},
            {"GITHUB_REF_PROTECTED", std::string{main_ctx.at("ref_protected").as_bool() ? "true" : "false"}},
            {"GITHUB_REF_TYPE", std::string{main_ctx.at("ref_type").as_string()}},
            {"GITHUB_REPOSITORY", std::string{main_ctx.at("repository").as_string()}},
            //{"GITHUB_REPOSITORY_ID", ""},
            {"GITHUB_REPOSITORY_OWNER", std::string{main_ctx.at("repository_owner").as_string()}},
            //{"GITHUB_REPOSITORY_OWNER_ID", ""},
            {"GITHUB_RETENTION_DAYS", std::string{main_ctx.at("retention_days").as_string()}},
            {"GITHUB_RUN_ATTEMPT", std::string{main_ctx.at("run_attempt").as_string()}},
            {"GITHUB_RUN_ID", std::string{main_ctx.at("run_id").as_string()}},
            {"GITHUB_RUN_NUMBER", std::string{main_ctx.at("run_number").as_string()}},
            {"GITHUB_SERVER_URL", std::string{main_ctx.at("server_url").as_string()}},
            {"GITHUB_SHA", std::string{main_ctx.at("sha").as_string()}},
            //{"GITHUB_STEP_SUMMARY", ""},
            {"GITHUB_TRIGGERING_ACTOR", std::string{main_ctx.at("triggering_actor").as_string()}},
            {"GITHUB_WORKFLOW", std::string{main_ctx.at("workflow").as_string()}},
            //{"GITHUB_WORKFLOW_REF", ""},
            //{"GITHUB_WORKFLOW_SHA", ""},
            {"GITHUB_WORKSPACE", std::string{main_ctx.at("workspace").as_string()}},
            {"RUNNER_ARCH", std::string{runner_ctx.at("arch").as_string()}},
            //{"RUNNER_DEBUG", ""},
            //{"RUNNER_ENVIRONMENT", ""},
            {"RUNNER_NAME", std::string{runner_ctx.at("name").as_string()}},
            {"RUNNER_OS", std::string{runner_ctx.at("os").as_string()}},
            {"RUNNER_TEMP", std::string{runner_ctx.at("temp").as_string()}},
            //{"RUNNER_TOOL_CACHE", ""},
            // Other variables
            {"GITHUB_TOKEN", std::string{main_ctx.at("token").as_string()}},
        };
        return env_ptr;
    } catch (const std::exception& ex) {
        return std::unexpected{generic_error{"Unable to generate initial environment variables"}};
    }
}

} // namespace ls_gitea_runner::gitea
