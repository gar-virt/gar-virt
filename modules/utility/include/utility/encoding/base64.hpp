#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ls_gitea_runner::utility {

std::string base64_encode(std::span<const std::byte> input);
std::vector<std::byte> base64_decode(std::string_view input);

} // namespace ls_gitea_runner::utility
