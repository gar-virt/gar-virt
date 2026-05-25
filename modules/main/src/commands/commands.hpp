#pragma once

#include "../config.hpp"
#include "../state.hpp"

namespace ls_gitea_runner {

int cmd_daemon(config::RunnerConfig config, RuntimeState state) noexcept;
int cmd_register(config::RunnerConfig config, RuntimeState state) noexcept;
int cmd_test(config::RunnerConfig config, RuntimeState state) noexcept;

} // namespace ls_gitea_runner
