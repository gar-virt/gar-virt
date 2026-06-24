#include "config.hpp"

#include <utility/algorithm.hpp>
#include <utility/string.hpp>

#include <boost/json.hpp>

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

std::expected<RunnerConfig, GenericError> load_file(const std::filesystem::path& file_path) noexcept {
    try {
        const auto config_base_dir{file_path.parent_path()};
        auto json{[&] {
            std::ifstream is{file_path, std::ios_base::binary};
            std::ostringstream ss{std::ios_base::binary};
            ss << is.rdbuf();
            return ss.str();
        }()};
        const auto j{boost::json::parse(json).as_object()};
        RunnerConfig c{
            .config_base_dir = config_base_dir,
            .instance_url = std::string{j.at("instance_url").as_string()},
            .token = std::string{j.at("token").as_string()},
            .name = std::string{j.at("name").as_string()},
            .labels =
                [&] {
                    const auto& runner_labels{j.at("labels").as_array()};
                    std::vector<std::string> labels;
                    labels.reserve(runner_labels.size());
                    std::transform(runner_labels.begin(), runner_labels.end(), std::back_inserter(labels),
                                   [](auto& l) { return std::string{l.as_string()}; });
                    return labels;
                }(),
            .ephemeral = j.at("ephemeral").as_bool(),
            .machine_pool =
                [&] {
                    const auto& pool{j.at("machine_pool").as_object()};
                    const std::string provider{pool.at("provider").as_string()};
                    const auto& pool_details{pool.at("details").as_object()};
                    return MachinePoolConfig{
                        .provider = provider,
                        .capacity = pool.at("capacity").as_int64(),
                        .os = std::string{pool.at("os").as_string()},
                        .arch = std::string{pool.at("arch").as_string()},
                        .temp_dir = std::string{pool.at("temp_dir").as_string()},
                        .workspaces_dir = std::string{pool.at("workspaces_dir").as_string()},
                        .runner_exe_path = std::string{pool.at("runner_exe_path").as_string()},
                        .details_as_json = boost::json::serialize(pool_details),
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
