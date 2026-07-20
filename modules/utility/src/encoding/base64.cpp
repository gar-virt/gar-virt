#include <utility/encoding/base64.hpp>

#include <string>

#include <cppcodec/base64_rfc4648.hpp>

namespace ls_gitea_runner::utility {

std::string base64_encode(std::span<const std::byte> input) { return cppcodec::base64_rfc4648::encode(input); }

std::vector<std::byte> base64_decode_to_bytes(std::string_view input) {
    std::vector<std::byte> output(cppcodec::base64_rfc4648::decoded_max_size(input.size()), std::byte{});
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    cppcodec::base64_rfc4648::decode(reinterpret_cast<uint8_t*>(output.data()), output.size(), input);
    return output;
}

std::string base64_decode_to_string(std::string_view input) {
    std::string output;
    cppcodec::base64_rfc4648::decode(output, input);
    return output;
}

} // namespace ls_gitea_runner::utility
