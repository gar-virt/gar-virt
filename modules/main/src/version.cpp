module;

#include <string_view>

export module main:version;

export namespace ls_gitea_runner {

constexpr auto runner_version{std::string_view{"v0.1.0"}};

}
