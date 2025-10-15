#pragma once

#include "../config.hpp"
#include "../error.hpp"
#include "gitea_runner_service_client.hpp"
#include "runner/v1/messages.pb.h"

#include <expected>
#include <memory>

namespace ls_gitea_runner::gitea {

class gitea_runner_task_processor final {
public:
    gitea_runner_task_processor(const gitea_runner_service_client& client, const config::runner_config& config);
    ~gitea_runner_task_processor();
    std::expected<void, generic_error> process(::runner::v1::Task task) noexcept;

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace ls_gitea_runner::gitea
