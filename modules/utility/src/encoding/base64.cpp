module;

#include <cppcodec/base64_rfc4648.hpp>

export module utility.encoding:base64;

import std;

namespace ls_gitea_runner::utility {

export std::string base64_encode(std::span<const std::byte> input) { return cppcodec::base64_rfc4648::encode(input); }

export std::vector<std::byte> base64_decode_to_bytes(std::string_view input) {
    std::vector<std::byte> output(cppcodec::base64_rfc4648::decoded_max_size(input.size()), std::byte{});
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    cppcodec::base64_rfc4648::decode(reinterpret_cast<uint8_t*>(output.data()), output.size(), input);
    return output;
}

export std::string base64_decode_to_string(std::string_view input) {
    std::string output;
    cppcodec::base64_rfc4648::decode(output, input);
    return output;
}

} // namespace ls_gitea_runner::utility
