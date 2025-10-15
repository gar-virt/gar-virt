#include "commands.hpp"

#include "ping/v1/messages.pb.h"
#include "runner/v1/messages.pb.h"

#include "../config.hpp"
#include "../gitea/gitea_runner_service_client.hpp"
#include "../version.hpp"

#include <cstring>
#include <expected>
#include <format>
#include <iostream>
#include <print>
#include <string>

namespace ls_gitea_runner {

int cmd_register(config::runner_config config, runtime_state state) noexcept {
    // GITEA_RUNNER_REGISTRATION_TOKEN_FILE

    gitea::gitea_runner_service_client client{config.instance_url};

    auto ping_request{::ping::v1::PingRequest{}};
    ping_request.set_data(config.name);
    auto ping_response{client.ping(ping_request)};
    if (!ping_response) {
        std::println(std::cerr, "Error");
        return 1;
    }

    auto reqister_request{::runner::v1::RegisterRequest{}};
    reqister_request.set_name(config.name);
    reqister_request.set_token(config.token);
    reqister_request.set_version(std::string{runner_version});
    for (auto& [env_type, env_config] : config.environments) {
        for (auto& label : env_config.labels) {
            reqister_request.add_labels(std::move(label));
        }
    }
    reqister_request.set_ephemeral(config.ephemeral);
    auto register_response{client.register_(reqister_request)};
    if (!register_response) {
        std::println(std::cerr, "Error");
        return 1;
    }

    state.uuid = register_response->runner().uuid();
    state.token = register_response->runner().token();

    if (auto res{state.save()}; !res) {
        std::println(std::cerr, "Unable to save state");
        return 1;
    }

    return 0;
}

} // namespace ls_gitea_runner
