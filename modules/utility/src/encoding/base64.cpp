#include "utility/encoding/base64.hpp"

#include <string>

#include <cppcodec/base64_rfc4648.hpp>

namespace ls_gitea_runner::utility {

std::string base64_encode(std::span<const std::byte> input) { return cppcodec::base64_rfc4648::encode(input); }

std::vector<std::byte> base64_decode(std::string_view input) {
    std::vector<uint8_t> output_u8;
    cppcodec::base64_rfc4648::decode(output_u8, input);
    auto* p{reinterpret_cast<const std::byte*>(output_u8.data())};
    return std::vector<std::byte>{p, p + output_u8.size() * sizeof(output_u8[0])};
}

} // namespace ls_gitea_runner::utility
