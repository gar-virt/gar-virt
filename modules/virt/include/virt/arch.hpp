#pragma once

#include <utility/error.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace ls_gitea_runner {

struct Arch {
    enum Type { amd64, arm64 };

    static std::expected<Arch::Type, GenericError> from_name(std::string_view name) noexcept;
    static std::string to_name(Arch::Type value) noexcept;
};

} // namespace ls_gitea_runner
