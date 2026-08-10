#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

namespace gv::utility {

template <typename T>
concept ByteLike = (std::integral<T> && !std::same_as<T, bool>) || std::same_as<T, std::byte>;

struct Base64DecodedLengthResult {
    std::size_t decoded_length{};
    std::size_t encoded_length{};
    std::size_t pad_length{};
};

namespace detail {
constexpr std::string_view alphabet{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
constexpr std::array<std::string_view, 3> pad_chars{"", "==", "="};
constexpr char pad_char{'='};
constexpr std::uint8_t decode_able_invalid_char{0xff};
constexpr std::uint32_t bitmask_6{(1 << 6) - 1};
constexpr std::uint32_t bitmask_8{(1 << 8) - 1};

/*
Maps a base64-encoded character to an index into the base64 alphabet.

Offset    | Char | Index
--------- | ---- | ---------
0x2b      | +    | 0x3e
0x2f      | /    | 0x3f
0x30-0x39 | 0-9  | 0x34-0x3d
0x3d      | =    | 0x41 (excluded)
0x41-0x5a | A-Z  | 0x00-0x19
0x61-0x7a | a-z  | 0x1a-0x33

0xff means invalid mapping.
*/
constexpr std::array<std::uint8_t, 256> decode_table = {
    //       0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08  0x09  0x0a  0x0b  0x0c  0x0d  0x0e  0x0f
    /*0x00*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0x10*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0x20*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    /*0x30*/ 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0x40*/ 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    /*0x50*/ 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0x60*/ 0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    /*0x70*/ 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0x80*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0x90*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0xa0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0xb0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0xc0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0xd0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0xe0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    /*0xf0*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

constexpr std::size_t calc_encoded_length_impl(std::size_t length) noexcept { return 4 * ((length + 2) / 3); }

template <typename T> Base64DecodedLengthResult calc_decoded_length_impl(const T& input) {
    if ((input.size() % 4) != 0) {
        throw std::runtime_error{"base64: invalid input length"};
    }
    std::size_t pad_length{};
    for (const auto input_char : input) {
        if (static_cast<std::uint8_t>(input_char) == static_cast<std::uint8_t>(pad_char)) {
            ++pad_length;
            continue;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): length cannot go out of bounds
        const auto mapped_char{decode_table[static_cast<std::uint8_t>(input_char)]};
        if (mapped_char == decode_able_invalid_char) {
            throw std::runtime_error{"base64: invalid input"};
        }
        if (pad_length > 0) {
            throw std::runtime_error{"base64: junk after padding"};
        }
    }
    if (pad_length > 2) {
        throw std::runtime_error{"base64: invalid padding"};
    }
    return {
        .decoded_length = ((input.size() / 4) * 3) - pad_length,
        .encoded_length = input.size(),
        .pad_length = pad_length,
    };
}

} // namespace detail

template <typename Output, typename Input>
    requires(ByteLike<typename Output::value_type> && ByteLike<typename Input::value_type>)
void base64_encode(Output& output, const Input& input) {
    using namespace detail;
    using OutputElement = typename Output::value_type;
    output.reserve(calc_encoded_length_impl(input.size()));
    std::size_t length{input.size()};
    std::size_t i{};
    for (; length >= 3; length -= 3) {
        const auto b0{std::uint32_t{static_cast<std::uint8_t>(input[i++])} << 16};
        const auto b1{std::uint32_t{static_cast<std::uint8_t>(input[i++])} << 8};
        const auto b2{std::uint32_t{static_cast<std::uint8_t>(input[i++])}};
        const auto bx{b0 | b1 | b2};
        const auto g0{(bx >> 18) & bitmask_6};
        const auto g1{(bx >> 12) & bitmask_6};
        const auto g2{(bx >> 6) & bitmask_6};
        const auto g3{bx & bitmask_6};
        output.push_back(static_cast<OutputElement>(alphabet[g0]));
        output.push_back(static_cast<OutputElement>(alphabet[g1]));
        output.push_back(static_cast<OutputElement>(alphabet[g2]));
        output.push_back(static_cast<OutputElement>(alphabet[g3]));
    }
    switch (length) {
    case 0:
        break;
    case 1: {
        const auto b0{std::uint32_t{static_cast<std::uint8_t>(input[i++])} << 16};
        const auto bx{b0};
        const auto g0{(bx >> 18) & bitmask_6};
        const auto g1{(bx >> 12) & bitmask_6};
        output.push_back(static_cast<OutputElement>(alphabet[g0]));
        output.push_back(static_cast<OutputElement>(alphabet[g1]));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): length cannot go out of bounds
        for (const auto p : pad_chars[length]) {
            output.push_back(static_cast<OutputElement>(p));
        }
        break;
    }
    case 2: {
        const auto b0{std::uint32_t{static_cast<std::uint8_t>(input[i++])} << 16};
        const auto b1{std::uint32_t{static_cast<std::uint8_t>(input[i++])} << 8};
        const auto bx{b0 | b1};
        const auto g0{(bx >> 18) & bitmask_6};
        const auto g1{(bx >> 12) & bitmask_6};
        const auto g2{(bx >> 6) & bitmask_6};
        output.push_back(static_cast<OutputElement>(alphabet[g0]));
        output.push_back(static_cast<OutputElement>(alphabet[g1]));
        output.push_back(static_cast<OutputElement>(alphabet[g2]));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): length cannot go out of bounds
        for (const auto p : pad_chars[length]) {
            output.push_back(static_cast<OutputElement>(p));
        }
        break;
    }
    default:
        std::unreachable();
    }
}

template <typename Output> void base64_encode(Output& output, std::string_view input) {
    base64_encode(output, std::span{input.data(), input.size()});
}

template <typename Output> void base64_encode(Output& output, std::u8string_view input) {
    base64_encode(output, std::span{input.data(), input.size()});
}

template <typename Output, typename Input>
    requires(ByteLike<typename Output::value_type> && ByteLike<typename Input::value_type>)
void base64_decode(Output& output, const Input& input) {
    using namespace detail;
    if (input.empty()) {
        return;
    }
    const auto calc_result{calc_decoded_length_impl(input)};
    output.reserve(calc_result.decoded_length);
    std::size_t input_length{calc_result.encoded_length - calc_result.pad_length};
    std::size_t i{};
    for (; input_length >= 4; input_length -= 4) {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index): length cannot go out of bounds
        const std::uint32_t g0{decode_table[static_cast<std::uint8_t>(input[i++])]};
        const std::uint32_t g1{decode_table[static_cast<std::uint8_t>(input[i++])]};
        const std::uint32_t g2{decode_table[static_cast<std::uint8_t>(input[i++])]};
        const std::uint32_t g3{decode_table[static_cast<std::uint8_t>(input[i++])]};
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
        const std::uint32_t bx{(g0 << 18) | (g1 << 12) | (g2 << 6) | g3};
        const auto b0{static_cast<typename Output::value_type>((bx >> 16) & bitmask_8)};
        const auto b1{static_cast<typename Output::value_type>((bx >> 8) & bitmask_8)};
        const auto b2{static_cast<typename Output::value_type>(bx & bitmask_8)};
        output.push_back(b0);
        output.push_back(b1);
        output.push_back(b2);
    }
    switch (input_length) {
    case 0:
        break;
    case 2: {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index): length cannot go out of bounds
        const std::uint32_t g0{decode_table[static_cast<std::uint8_t>(input[i++])]};
        const std::uint32_t g1{decode_table[static_cast<std::uint8_t>(input[i++])]};
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
        const std::uint32_t bx{(g0 << 18) | (g1 << 12)};
        const auto b0{static_cast<typename Output::value_type>((bx >> 16) & bitmask_8)};
        output.push_back(b0);
        break;
    }
    case 3: {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index): length cannot go out of bounds
        const std::uint32_t g0{decode_table[static_cast<std::uint8_t>(input[i++])]};
        const std::uint32_t g1{decode_table[static_cast<std::uint8_t>(input[i++])]};
        const std::uint32_t g2{decode_table[static_cast<std::uint8_t>(input[i++])]};
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
        const std::uint32_t bx{(g0 << 18) | (g1 << 12) | (g2 << 6)};
        const auto b0{static_cast<typename Output::value_type>((bx >> 16) & bitmask_8)};
        const auto b1{static_cast<typename Output::value_type>((bx >> 8) & bitmask_8)};
        output.push_back(b0);
        output.push_back(b1);
        break;
    }
    default:
        std::unreachable();
    }
}

template <typename Output> void base64_decode(Output& output, std::string_view input) {
    base64_decode(output, std::span{input.data(), input.size()});
}

template <typename Output> void base64_decode(Output& output, std::u8string_view input) {
    base64_decode(output, std::span{input.data(), input.size()});
}

template <typename R, typename T> R base64_encode_return(const T& input) {
    R output;
    base64_encode(output, input);
    return output;
}

template <typename R> R base64_encode_return(std::string_view input) {
    return base64_encode_return<R>(std::span{input.data(), input.size()});
}

template <typename R> R base64_encode_return(std::u8string_view input) {
    return base64_encode_return<R>(std::span{input.data(), input.size()});
}

template <typename R, typename T> R base64_decode_return(const T& input) {
    R output;
    base64_decode(output, input);
    return output;
}

template <typename R> R base64_decode_return(std::string_view input) {
    return base64_decode_return<R>(std::span{input.data(), input.size()});
}

template <typename R> R base64_decode_return(std::u8string_view input) {
    return base64_decode_return<R>(std::span{input.data(), input.size()});
}

} // namespace gv::utility

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
