#include "utility/string.hpp"

#ifdef _WIN32
    #define NOMINMAX
    #include <stdexcept>
    #include <windows.h>
#endif

#include <algorithm>

namespace ls_gitea_runner::utility {

#if defined(_WIN32)
// Converts a narrow (UTF-8-encoded) string into a wide (UTF-16-encoded) string.
std::wstring widen_string(const char* data, size_t length) {
    if (length == 0) {
        return {};
    }
    const UINT cp{CP_UTF8};
    const DWORD flags{MB_ERR_INVALID_CHARS};
    const auto input_length{utility::safe_cast_int<int>(length)};
    auto required_length{::MultiByteToWideChar(cp, flags, data, input_length, nullptr, 0)};
    if (required_length > 0) {
        std::wstring output(utility::safe_cast_int<std::size_t>(required_length), L'\0');
        if (::MultiByteToWideChar(cp, flags, data, input_length, &output[0], required_length) > 0) {
            return output;
        }
    }
    throw std::runtime_error{"Failed to convert string from UTF-8 to UTF-16"};
}

std::wstring widen_string(const std::string& input) { return widen_string(input.data(), input.size()); }

// Converts a wide (UTF-16-encoded) string into a narrow (UTF-8-encoded) string.
std::string narrow_string(const wchar_t* data, size_t length) {
    struct wc_flags {
        enum TYPE : unsigned int {
            // WC_ERR_INVALID_CHARS
            err_invalid_chars = 0x00000080U
        };
    };
    if (input.empty()) {
        return {};
    }
    const UINT cp{CP_UTF8};
    const DWORD flags{wc_flags::err_invalid_chars};
    const auto input_length{utility::safe_cast_int<int>(length)};
    const auto required_length{WideCharToMultiByte(cp, flags, data, input_length, nullptr, 0, nullptr, nullptr)};
    if (required_length > 0) {
        std::basic_string<T> output(utility::safe_cast_int<std::size_t>(required_length), '\0');
        if (WideCharToMultiByte(cp, flags, data, input_length, reinterpret_cast<char*>(&output[0]), required_length,
                                nullptr, nullptr) > 0) {
            return output;
        }
    }
    throw std::runtime_error{"Failed to convert string from UTF-8 to UTF-16"};
}

std::string narrow_string(const std::wstring& input) { return narrow_string(input.data(), input.size()); }
#endif

std::string string_from_u8string(const std::u8string_view from) {
    std::string result;
    result.reserve(from.size());
    for (auto c : from) {
        result.push_back(static_cast<char>(c));
    }
    return result;
}

const std::string_view string_trim_left(const std::string_view s, const std::set<char> chars) {
    for (std::size_t i{}; i < s.size(); ++i) {
        if (!chars.contains(s[i])) {
            return s.substr(i, s.size() - i);
        }
    }
    return s;
}

const std::string_view string_trim_right(const std::string_view s, const std::set<char> chars) {
    for (std::size_t i{}; i < s.size(); ++i) {
        const auto j{s.size() - i};
        if (!chars.contains(s[j - 1])) {
            return s.substr(0, j);
        }
    }
    return s;
}

const std::string_view string_trim(const std::string_view s, const std::set<char> chars) {
    return string_trim_left(string_trim_right(s, chars), chars);
}

void string_trim_right(std::in_place_t, std::string& s, const std::set<char> chars) {
    auto length{s.size()};
    for (auto it{s.rbegin()}; it != s.rend(); ++it) {
        if (!chars.contains(*it)) {
            break;
        }
        --length;
    }
    if (length != s.size()) {
        s.resize(length);
    }
}

void string_trim_right(std::in_place_t, std::string& s, char c) {
    return string_trim_right(std::in_place, s, std::set<char>{c});
}

bool string_contains_ci(const std::string_view needle, const std::string_view haystack) {
    if (needle.empty() && haystack.empty()) {
        return true;
    }
    if (haystack.empty()) {
        return false;
    }
    const auto match{std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                                 [](auto a, auto b) { return std::toupper(a) == std::toupper(b); })};
    return match != haystack.end();
}

int string_compare_ci(const std::string_view first, const std::string_view second) {
    const auto length{std::min(first.size(), second.size())};
    for (size_t i{}; i < length; ++i) {
        const auto a{std::toupper(first[i])};
        const auto b{std::toupper(second[i])};
        if (a < b) {
            return -1;
        }
        if (a > b) {
            return 1;
        }
    }
    if (first.size() < second.size()) {
        return -1;
    }
    if (first.size() > second.size()) {
        return 1;
    }
    return 0;
}

bool string_compare_less_ci(const std::string_view first, const std::string_view second) {
    return std::lexicographical_compare(first.begin(), first.end(), second.begin(), second.end(),
                                        [](auto a, auto b) { return std::toupper(a) < std::toupper(b); });
}

bool string_equals_ci(const std::string_view first, const std::string_view second) {
    return std::equal(first.begin(), first.end(), second.begin(), second.end(),
                      [](auto a, auto b) { return std::toupper(a) == std::toupper(b); });
}

bool string_starts_with(const std::string_view haystack, const std::string_view needle) {
    if (haystack.size() < needle.size()) {
        return false;
    }
    if (haystack.empty() && needle.empty()) {
        return true;
    }
    for (size_t i{}; i < needle.size(); ++i) {
        if (haystack[i] != needle[i]) {
            return false;
        }
    }
    return true;
}

bool string_ends_with(const std::string_view haystack, const std::string_view needle) {
    if (haystack.size() < needle.size()) {
        return false;
    }
    if (haystack.empty() && needle.empty()) {
        return true;
    }
    for (size_t i{haystack.size() - 1}, j{needle.size() - 1}; j > 0; --i, --j) {
        if (haystack[i] != needle[j]) {
            return false;
        }
    }
    return true;
}

void string_split(const std::string_view input, char separator, std::function<void(const std::string_view token)> cb) {
    auto result{std::vector<std::string>{}};
    std::string::size_type a{}, b{};
    for (; (b = input.find_first_of(separator, b)) != std::string::npos; a = ++b) {
        cb(input.substr(a, b - a));
    }
    if (input.length() - a > 0U) {
        cb(input.substr(a));
    }
}

std::vector<std::string> string_split(const std::string_view input, char separator) {
    auto result{std::vector<std::string>{}};
    std::string::size_type a{}, b{};
    for (; (b = input.find_first_of(separator, b)) != std::string::npos; a = ++b) {
        result.emplace_back(input.substr(a, b - a));
    }
    if (input.length() - a > 0U) {
        result.emplace_back(input.substr(a));
    }
    return result;
}

// This function was generated by Grok
std::string regex_replace_callable(const std::string& text, const std::regex& pattern,
                                   std::function<std::string(const std::smatch&)> replacer) {
    std::string result;
    auto begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    auto end = std::sregex_iterator();
    size_t last_pos = 0;
    for (auto it = begin; it != end; ++it) {
        auto match = *it;
        result += text.substr(last_pos, match.position() - last_pos);
        result += replacer(match);
        last_pos = match.position() + match.length();
    }
    result += text.substr(last_pos);
    return result;
}

} // namespace ls_gitea_runner::utility
