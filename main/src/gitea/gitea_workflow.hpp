#pragma once

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

struct wf_step_run {
    std::optional<std::string> shell;
    std::string script;
    std::optional<std::filesystem::path> working_directory;
};

struct wf_step_uses {
    std::string url;
};

struct wf_step {
    std::optional<std::string> name;
    std::optional<wf_step_run> run;
    std::optional<wf_step_uses> uses;
    std::optional<std::string> condition;
    YAML::Node yaml;
};

struct wf_job {
    std::vector<wf_step> steps;
    std::optional<std::string> condition;
};

struct wf_run_contexts {
    std::string main;    // main context as a JSON object
    std::string vars;    // configuration variables  as a JSON object
    std::string secrets; // secrets  as a JSON object
    std::string runner;  // info about current runner
    std::string matrix;  // current matrix combination

    std::vector<std::pair<std::string, std::string>> to_list() const;
};

std::vector<wf_step> wf_load_steps(const YAML::Node& yaml_steps, const wf_run_contexts& contexts);

std::expected<YAML::Node, generic_error> wf_load_yaml(const std::string& yaml_str) noexcept;

std::expected<YAML::Node, generic_error> wf_find_job_with_name_in_yaml(const YAML::Node& yaml,
                                                                       const std::string& name) noexcept;

std::expected<wf_job, generic_error> wf_load_job_with_name(const YAML::Node& yaml, const std::string& name,
                                                           const wf_run_contexts& contexts) noexcept;

using wf_env_vars = std::shared_ptr<std::unordered_map<std::string, std::string>>;

std::string wf_load_matrix_context_from_job_yaml(const YAML::Node& yaml);

std::string wf_create_runner_context();

wf_env_vars wf_load_and_derive_env_from_yaml(const YAML::Node& yaml, wf_env_vars env, const wf_run_contexts& contexts);

} // namespace ls_gitea_runner::gitea
