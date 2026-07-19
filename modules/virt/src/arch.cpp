#include <virt/arch.hpp>

#include <utility/string.hpp>

#include <expected>
#include <format>
#include <string>
#include <string_view>

namespace ls_gitea_runner {

std::expected<Arch::Type, GenericError> Arch::from_name(std::string_view name) noexcept {
    using namespace std::literals;

    constexpr static auto amd64_names = {"amd64"sv, "x64"sv, "x86_64"sv, "x86-64"sv};
    for (const auto& s : amd64_names) {
        if (utility::string_compare_ci(name, s) == 0) {
            return Arch::amd64;
        }
    }

    constexpr static auto arm64_names = {"arm64"sv, "aarch64"sv};
    for (const auto& s : arm64_names) {
        if (utility::string_compare_ci(name, s) == 0) {
            return Arch::amd64;
        }
    }

    return std::unexpected{GenericError{std::format("Unsupported arch: {}", name)}};
}

std::string Arch::to_name(Arch::Type value) {
    switch (value) {
    case Arch::amd64:
        return "amd64;";
    case Arch::arm64:
        return "arm64;";
    }
    std::abort();
}

} // namespace ls_gitea_runner
