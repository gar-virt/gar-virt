#include "config.hpp"

#include <utility/algorithm.hpp>
#include <utility/env.hpp>
#include <utility/filesystem.hpp>
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

ForgeTokenConfig load_forge_token(const YAML::Node& from) {
    return {
        .source = std::string{from["source"].as<std::string>()},
        .value = std::string{from["value"].as<std::string>()},
    };
}

ForgeConfig load_forge(const YAML::Node& from, const std::filesystem::path& config_base_dir) {
    ForgeConfig result{
        .type = std::string{from["type"].as<std::string>()},
        .uri = std::string{from["uri"].as<std::string>()},
        .token_config = load_forge_token(from["token"]),
    };
    result.token = result.token_config.resolve(config_base_dir);
    return result;
}

std::string ForgeTokenConfig::resolve(const std::filesystem::path& base_dir) {
    if (source == "inline") {
        return value;
    }
    if (source == "env") {
        return utility::getenv(value).value();
    }
    if (source == "file") {
        std::filesystem::path file_path{utility::u8string_from_string(value)};
        if (file_path.is_relative()) {
            file_path = base_dir / file_path;
        }
        auto raw_content{fs::read_file<std::string>(file_path)};
        auto trimmed_content{utility::string_trim(raw_content)};
        return raw_content.size() > trimmed_content.size() ? std::string{trimmed_content} : raw_content;
    }
    return {};
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
            .config_version = utility::safe_cast_int<size_t>(y["config_version"].as<int>()),
            .name = std::string{y["name"].as<std::string>()},
            .forge = load_forge(y["forge"], config_base_dir),
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
