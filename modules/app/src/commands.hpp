#pragma once

#include "config.hpp"

#include <utility/error.hpp>

#include <expected>

namespace gv {

std::expected<void, GenericError> cmd_daemon(config::MainConfig config);

} // namespace gv
