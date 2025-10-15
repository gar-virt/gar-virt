#include "config.hpp"

#include <utility/algorithm.hpp>
#include <utility/string.hpp>

#include <boost/json.hpp>

#include <fstream>

namespace ls_gitea_runner::config {

std::expected<runner_config, generic_error> load_file(const std::filesystem::path& file_path) noexcept {
    try {
        std::ifstream is{file_path, std::ios_base::binary};
        const auto j{boost::json::parse(is).as_object()};
        runner_config c{
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
                .details = [&] -> decltype(runner_environment_config::details) {
                    if (env_key == "docker") {
                        return docker_config{
                            .image = std::string{env_details.at("image").as_string()},
                            .tag = std::string{env_details.at("tag").as_string()},
                        };
                    } else if (env_key == "qemu") {
                        return qemu_config{
                            .image = std::string{env_details.at("image").as_string()},
                            .cpu = std::string{env_details.at("cpu").as_string()},
                            .memory = utility::safe_cast_int<decltype(qemu_config::memory)>(
                                env_details.at("memory").as_int64()),
                        };
                    }
                    throw generic_error{"Invalid runner environment type in config"};
                }(),
            };
        }
        return c;
    } catch (const std::exception& ex) {
        return std::unexpected{
            generic_error{std::format("Error while loading config file \"{}\": {}",
                                      utility::string_from_u8string(file_path.u8string()), ex.what())}};
    }
}

} // namespace ls_gitea_runner::config
