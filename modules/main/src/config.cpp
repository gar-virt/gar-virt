#include "config.hpp"

#include <utility/algorithm.hpp>
#include <utility/string.hpp>

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace ls_gitea_runner::config {

std::vector<std::string> RunnerConfig::get_label_names() const {
    std::vector<std::string> items;
    for (auto label : labels) {
        auto pos{label.find_first_of(':')};
        if (pos != std::string::npos) {
            label = label.substr(0, pos);
        }
        items.push_back(std::move(label));
    }
    return items;
}

std::expected<YAML::Node, GenericError> load_yaml_file(const std::filesystem::path& file_path) noexcept {
    try {
        std::ifstream is{file_path, std::ios_base::binary};
        return YAML::Load(is);
    } catch (const std::exception&) {
        return std::unexpected{GenericError{"Failed to parse workflow YAML"}};
    }
}

std::expected<RunnerConfig, GenericError> load_file(const std::filesystem::path& file_path) noexcept {
    try {
        const auto config_base_dir{file_path.parent_path()};
        auto yaml_res{load_yaml_file(file_path)};
        if (!yaml_res) {
            return std::unexpected{yaml_res.error()};
        }
        const auto& y{*yaml_res};
        RunnerConfig c{
            .config_base_dir = config_base_dir,
            .instance_url = std::string{y["instance_url"].as<std::string>()},
            .token = std::string{y["token"].as<std::string>()},
            .name = std::string{y["name"].as<std::string>()},
            .labels =
                [&] {
                    const auto& runner_labels{y["labels"]};
                    std::vector<std::string> labels;
                    labels.reserve(runner_labels.size());
                    std::transform(runner_labels.begin(), runner_labels.end(), std::back_inserter(labels),
                                   [](const YAML::Node& l) { return l.as<std::string>(); });
                    return labels;
                }(),
            .machine_pool =
                [&] {
                    const auto& pool{y["machine_pool"]};

                    auto details_as_yaml{[&] {
                        std::stringstream details_as_yaml;
                        YAML::Emitter details_as_yaml_emitter{details_as_yaml};
                        details_as_yaml_emitter << pool["details"];
                        return std::move(details_as_yaml).str();
                    }()};

                    return MachinePoolConfig{
                        .provider = pool["provider"].as<std::string>(),
                        .capacity = utility::safe_cast_int<size_t>(pool["capacity"].as<int>()),
                        .os = pool["os"].as<std::string>(),
                        .arch = pool["arch"].as<std::string>(),
                        .temp_dir = pool["temp_dir"].as<std::string>(),
                        .workspaces_dir = pool["workspaces_dir"].as<std::string>(),
                        .runner_exe_path = pool["runner_exe_path"].as<std::string>(),
                        .details_as_yaml = std::move(details_as_yaml),
                    };
                }(),
        };
        return c;
    } catch (const std::exception& ex) {
        return std::unexpected{
            GenericError{std::format("Error while loading config file \"{}\": {}",
                                     utility::string_from_u8string(file_path.u8string()), ex.what())}};
    }
}

} // namespace ls_gitea_runner::config
