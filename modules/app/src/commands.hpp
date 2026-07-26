#pragma once

#include "config.hpp"

#include <utility/result.hpp>

#include <expected>

namespace gv {

Result<void> cmd_daemon(config::MainConfig config);

} // namespace gv
