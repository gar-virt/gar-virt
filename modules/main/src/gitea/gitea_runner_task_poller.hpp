#pragma once

#include "../config.hpp"
#include "gitea_runner_service_client.hpp"
#include "runner/v1/messages.pb.h"

#include <expected>
#include <functional>
#include <memory>

namespace ls_gitea_runner::gitea {

class gitea_runner_task_poller final {
public:
    using run_callback_fn = std::function<void(::runner::v1::Task)>;

    gitea_runner_task_poller(const gitea_runner_service_client& client, const config::runner_config& config,
                             run_callback_fn cb);
    ~gitea_runner_task_poller();

    gitea_runner_task_poller(const gitea_runner_task_poller&) = delete;
    gitea_runner_task_poller& operator=(const gitea_runner_task_poller&) = delete;

    void run();

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace ls_gitea_runner::gitea
