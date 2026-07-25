module app:commands;

import :config;

import utility.misc;

import std;

namespace ls_gitea_runner {

std::expected<void, GenericError> cmd_daemon(config::MainConfig main_config);

} // namespace ls_gitea_runner
