#pragma once

#include <string>

namespace gv::gitea {

struct GiteaRunnerCredentials {
    std::string uuid;
    std::string token;
};

} // namespace gv::gitea
