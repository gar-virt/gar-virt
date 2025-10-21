#pragma once

#include "../config.hpp"
#include "../state.hpp"

namespace ls_gitea_runner {

int cmd_daemon(config::runner_config config, runtime_state state) noexcept;
int cmd_register(config::runner_config config, runtime_state state) noexcept;
int cmd_test(config::runner_config config, runtime_state state) noexcept;

} // namespace ls_gitea_runner
