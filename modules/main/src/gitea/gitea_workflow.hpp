#pragma once

#include "../config.hpp"
#include "../error.hpp"

#include <yaml-cpp/yaml.h>

#include <cstring>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ls_gitea_runner::gitea {

struct WfStepRun {
    std::optional<std::string> shell;
    std::string script;
    std::optional<std::filesystem::path> working_directory;
};

struct WfStepUses {
    std::string url;
};

struct WfStep {
    std::optional<std::string> name;
    std::optional<WfStepRun> run;
    std::optional<WfStepUses> uses;
    std::optional<std::string> condition;
    YAML::Node yaml;
};

struct WfJob {
    std::vector<WfStep> steps;
    std::optional<std::string> condition;
};

struct WfRunContexts {
    std::string main;    // main context as a JSON object
    std::string vars;    // configuration variables  as a JSON object
    std::string secrets; // secrets  as a JSON object
    std::string runner;  // info about current runner
    std::string matrix;  // current matrix combination

    std::vector<std::pair<std::string, std::string>> to_list() const;
};

std::vector<WfStep> wf_load_steps(const YAML::Node& yaml_steps, const WfRunContexts& contexts);

std::expected<YAML::Node, GenericError> wf_load_yaml(const std::string& yaml_str) noexcept;

std::expected<YAML::Node, GenericError> wf_find_job_with_name_in_yaml(const YAML::Node& yaml,
                                                                      const std::string& name) noexcept;

std::expected<WfJob, GenericError> wf_load_job_with_name(const YAML::Node& yaml, const std::string& name,
                                                         const WfRunContexts& contexts) noexcept;

std::expected<std::string, GenericError> wf_get_label_from_job_yaml(const YAML::Node& yaml) noexcept;

using wf_env_vars = std::shared_ptr<std::unordered_map<std::string, std::string>>;

std::string wf_load_matrix_context_from_job_yaml(const YAML::Node& yaml);

std::string wf_create_runner_context(const std::string& name, const config::RunnerEnvironmentConfig& config);

wf_env_vars wf_load_and_derive_env_from_yaml(const YAML::Node& yaml, wf_env_vars env, const WfRunContexts& contexts);
std::expected<wf_env_vars, GenericError> wf_create_initial_env(const WfRunContexts& contexts);

} // namespace ls_gitea_runner::gitea
