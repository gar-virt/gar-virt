#pragma once

#include "../config.hpp"

#include "utility/error.hpp"

#include <expected>

namespace ls_gitea_runner {

std::expected<void, GenericError> cmd_daemon(config::RunnerConfig config) noexcept;

} // namespace ls_gitea_runner
