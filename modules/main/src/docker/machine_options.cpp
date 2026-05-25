#include "machine_options.hpp"

#include <boost/json.hpp>

#include <format>

namespace ls_gitea_runner {

std::expected<DockerMachineOptions, GenericError> DockerMachineOptions::load(const std::string& json_str) {
    try {
        const auto j{boost::json::parse(json_str)};
        const auto& j_obj{j.as_object()};
        return DockerMachineOptions{.image = std::string{j_obj.at("image").as_string()}};
    } catch (const std::exception& ex) {
        return std::unexpected{std::format("Unable to parse Docker machine options: {}", ex.what())};
    }
}

} // namespace ls_gitea_runner
