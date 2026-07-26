#pragma once

#include <utility/result.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace gv {

struct Arch {
    enum Type { amd64, arm64 };

    static Result<Arch::Type> from_name(std::string_view name) noexcept;
    static std::string to_name(Arch::Type value);
};

} // namespace gv
