module;

#include <string_view>

export module main:version;

namespace ls_gitea_runner {

export constexpr auto runner_version{std::string_view{"v0.1.0"}};

}
