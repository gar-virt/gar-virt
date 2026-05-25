#pragma once

#include <string>

namespace ls_gitea_runner::gitea {

struct GiteaRunnerCredentials {
    std::string uuid;
    std::string token;
};

} // namespace ls_gitea_runner::gitea
