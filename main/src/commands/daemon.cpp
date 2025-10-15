#include "commands.hpp"

#include "runner/v1/messages.pb.h"

#include "../config.hpp"
#include "../gitea/gitea_runner_service_client.hpp"
#include "../gitea/gitea_runner_task_poller.hpp"
#include "../gitea/gitea_runner_task_processor.hpp"
#include "../version.hpp"

#include <cstring>
#include <expected>
#include <format>
#include <iostream>
#include <print>
#include <string>

namespace ls_gitea_runner {

int cmd_daemon(config::runner_config config, runtime_state state) noexcept {
    gitea::gitea_runner_service_client client{config.instance_url};
    client.set_credentials(gitea::gitea_runner_credentials{.uuid = state.uuid, .token = state.token});
    gitea::gitea_runner_task_processor processor{client, config};
    gitea::gitea_runner_task_poller poller{client, config, [&](auto task) {
                                               const auto id{task.id()};
                                               if (auto res{processor.process(std::move(task))}; !res) {
                                                   std::println(std::cerr, "Task #{} failed: {}", id,
                                                                res.error().what());
                                               }
                                           }};

    auto declare_request{::runner::v1::DeclareRequest{}};
    declare_request.set_version(std::string{runner_version});
    for (auto& [env_type, env_config] : config.environments) {
        for (auto& label : env_config.labels) {
            declare_request.add_labels(std::move(label));
        }
    }
    auto declare_response{client.declare(declare_request)};
    if (!declare_response) {
        std::println(std::cerr, "Failed to declare runner: {}", declare_response.error().what());
        return 1;
    }

    poller.run();

    return 0;
}

} // namespace ls_gitea_runner
