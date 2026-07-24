module;

#include <yaml-cpp/yaml.h>

module app:config;

import utility.fs;
import utility.log;
import utility.misc;
import virt;

import std;

namespace ls_gitea_runner::config {
namespace {

std::expected<YAML::Node, GenericError> load_yaml_file(const std::filesystem::path& file_path) {
    try {
        std::ifstream is{file_path, std::ios_base::binary};
        return YAML::Load(is);
    } catch (const std::exception&) {
        return std::unexpected{GenericError{"Failed to parse workflow YAML"}};
    }
}

std::string to_yaml_string(const YAML::Node& from) {
    std::stringstream result;
    YAML::Emitter emitter{result};
    emitter << from;
    return std::move(result).str();
}

ForgeTokenConfig load_forge_token(const YAML::Node& from) {
    return {
        .source = std::string{from["source"].as<std::string>()},
        .value = std::string{from["value"].as<std::string>()},
    };
}

ForgeConfig load_forge(const YAML::Node& from) {
    return {
        .type = std::string{from["type"].as<std::string>()},
        .uri = std::string{from["uri"].as<std::string>()},
        .token = load_forge_token(from["token"]),
    };
}

MachineTemplateConfig load_template(const YAML::Node& from) {
    auto arch{Arch::from_name(from["arch"].as<std::string>())};
    if (!arch) {
        throw std::move(arch).error();
    }
    return {
        .os = from["os"].as<std::string>(),
        .arch = arch.value(),
        .temp_dir = from["temp_dir"].as<std::string>(),
        .idle_target = utility::safe_cast_int<size_t>(from["idle_target"].as<int>()),
        .max_concurrency = utility::safe_cast_int<size_t>(from["max_concurrency"].as<int>()),
        .runner_exe_path = from["runner_exe_path"].as<std::string>(),
        .labels =
            [&] {
                const auto& runner_labels{from["labels"]};
                std::vector<std::string> labels;
                labels.reserve(runner_labels.size());
                std::ranges::transform(runner_labels, std::back_inserter(labels),
                                       [](const YAML::Node& l) { return l.as<std::string>(); });
                return labels;
            }(),
        .raw_details = to_yaml_string(from["details"]),
    };
}

void load_templates_into(std::vector<MachineTemplateConfig>& to, const YAML::Node& from) {
    for (const auto& from_element : from) {
        to.push_back(load_template(from_element));
    }
}

BackendConfig load_backend(const YAML::Node& from) {
    BackendConfig c{
        .type = from["type"].as<std::string>(),
        .name = from["name"].as<std::string>(),
        .raw_details = to_yaml_string(from["details"]),
    };
    load_templates_into(c.templates, from["templates"]);
    return c;
}

void load_backends_into(std::vector<BackendConfig>& to, const YAML::Node& from) {
    for (const auto& from_element : from) {
        to.push_back(load_backend(from_element));
    }
}

utility::LogLevel load_log_level(const YAML::Node& from) {
    const auto level_name = from.as<std::string>();
    const auto is{[&](std::string_view other) { return utility::string_compare_ci(level_name, other) == 0; }};
    if (is("none")) {
        return utility::LogLevel::none;
    }
    if (is("error")) {
        return utility::LogLevel::error;
    }
    if (is("warn")) {
        return utility::LogLevel::warning;
    }
    if (is("info")) {
        return utility::LogLevel::info;
    }
    if (is("debug")) {
        return utility::LogLevel::debug;
    }
    throw GenericError{std::format("Invalid log level: {}", level_name)};
}

LogConfig load_log(const YAML::Node& from) {
    return LogConfig{
        .level = load_log_level(from["level"]),
    };
}

} // namespace

std::expected<MainConfig, GenericError> load_file(const std::filesystem::path& file_path) {
    try {
        const auto base_dir{file_path.parent_path()};
        auto yaml_res{load_yaml_file(file_path)};
        if (!yaml_res) {
            return std::unexpected{yaml_res.error()};
        }
        const auto& y{*yaml_res};
        MainConfig c{
            .config_version = utility::safe_cast_int<size_t>(y["config_version"].as<int>()),
            .log = load_log(y["log"]),
            .name = std::string{y["name"].as<std::string>()},
            .forge = load_forge(y["forge"]),
        };
        load_backends_into(c.backends, y["backends"]);
        c.resolve(base_dir);
        return c;
    } catch (const std::exception& ex) {
        return std::unexpected{
            GenericError{std::format("Error while loading config file \"{}\": {}",
                                     utility::string_from_u8string(file_path.u8string()), ex.what())}};
    }
}

} // namespace ls_gitea_runner::config
