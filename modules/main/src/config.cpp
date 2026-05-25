#include "config.hpp"

#include <utility/algorithm.hpp>
#include <utility/string.hpp>

#include <boost/json.hpp>

#include <fstream>

namespace ls_gitea_runner::config {

std::optional<std::tuple<std::string, RunnerEnvironmentConfig>>
RunnerConfig::find_environment_by_label(const std::string_view search_label) const noexcept {
    for (auto& [env_k, env_v] : environments) {
        for (auto& label : env_v.labels) {
            if (utility::string_equals_ci(label, search_label)) {
                return std::make_tuple(env_k, env_v);
            }
        }
    }
    return std::nullopt;
}

std::expected<RunnerConfig, GenericError> load_file(const std::filesystem::path& file_path) noexcept {
    try {
        auto json{[&] {
            std::ifstream is{file_path, std::ios_base::binary};
            std::ostringstream ss{std::ios_base::binary};
            ss << is.rdbuf();
            return ss.str();
        }()};
        const auto j{boost::json::parse(json).as_object()};
        RunnerConfig c{
            .instance_url = std::string{j.at("instance_url").as_string()},
            .name = std::string{j.at("name").as_string()},
            .token = std::string{j.at("token").as_string()},
            .ephemeral = j.at("ephemeral").as_bool(),
        };
        for (auto& [env_key, env_value] : j.at("environments").as_object()) {
            const auto& env_labels{env_value.at("labels").as_array()};
            const auto& env_details{env_value.at("details").as_object()};
            c.environments[env_key] = {
                .labels =
                    [&] {
                        std::vector<std::string> labels;
                        labels.reserve(env_labels.size());
                        std::transform(env_labels.begin(), env_labels.end(), std::back_inserter(labels),
                                       [](auto& l) { return std::string{l.as_string()}; });
                        return labels;
                    }(),
                .os = std::string{env_value.at("os").as_string()},
                .arch = std::string{env_value.at("arch").as_string()},
                .temp_dir = std::string{env_value.at("temp_dir").as_string()},
                .workspaces_dir = std::string{env_value.at("workspaces_dir").as_string()},
                .details = [&] -> decltype(RunnerEnvironmentConfig::details) {
                    if (env_key == "docker") {
                        return DockerConfig{
                            .image = std::string{env_details.at("image").as_string()},
                            .tag = std::string{env_details.at("tag").as_string()},
                        };
                    } else if (env_key == "qemu") {
                        return QemuConfig{
                            .image = std::string{env_details.at("image").as_string()},
                            .cpu = std::string{env_details.at("cpu").as_string()},
                            .memory = utility::safe_cast_int<decltype(QemuConfig::memory)>(
                                env_details.at("memory").as_int64()),
                        };
                    }
                    throw GenericError{"Invalid runner environment type in config"};
                }(),
                .details_as_json = boost::json::serialize(env_details),
            };
        }
        return c;
    } catch (const std::exception& ex) {
        return std::unexpected{
            GenericError{std::format("Error while loading config file \"{}\": {}",
                                     utility::string_from_u8string(file_path.u8string()), ex.what())}};
    }
}

} // namespace ls_gitea_runner::config
