#pragma once

#include <string>

namespace ls_gitea_runner::gitea {

struct gitea_runner_credentials {
    std::string uuid;
    std::string token;
};

} // namespace ls_gitea_runner::gitea
