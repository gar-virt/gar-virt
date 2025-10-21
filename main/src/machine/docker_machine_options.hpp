#pragma once

#include "../error.hpp"

#include <expected>
#include <string>

namespace ls_gitea_runner {

struct docker_machine_options {
    std::string image;

    static std::expected<docker_machine_options, generic_error> load(const std::string& json_str);
};

} // namespace ls_gitea_runner
