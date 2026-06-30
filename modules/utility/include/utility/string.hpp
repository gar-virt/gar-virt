#pragma once

#include <functional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ls_gitea_runner::utility {

#if defined(_WIN32)
// Converts a narrow (UTF-8-encoded) string into a wide (UTF-16-encoded) string.
std::wstring widen_string(const char* data, size_t length);

std::wstring widen_string(const std::string& input);

// Converts a wide (UTF-16-encoded) string into a narrow (UTF-8-encoded) string.
std::string narrow_string(const wchar_t* data, size_t length);

std::string narrow_string(const std::wstring& input);
#endif

std::u8string u8string_from_string(const std::string& from);
std::u8string u8string_from_string(std::string_view from);
std::string string_from_u8string(const std::u8string& from);
std::string string_from_u8string(std::u8string_view from);
std::string_view string_trim_left(std::string_view s, std::set<char> chars = {'\t', '\n', '\f', '\r', ' '});
std::string_view string_trim_right(std::string_view s, std::set<char> chars = {'\t', '\n', '\f', '\r', ' '});
std::string_view string_trim(std::string_view s, std::set<char> chars = {'\t', '\n', '\f', '\r', ' '});
void string_trim_right(std::in_place_t, std::string& s, std::set<char> chars = {'\t', '\n', '\f', '\r', ' '});
void string_trim_right(std::in_place_t, std::string& s, char c);

template <typename Container>
    requires(!std::is_convertible_v<Container, std::string_view>)
std::string string_join(std::string_view glue, const Container& container) {
    std::string joined;
    for (auto i = size_t{}; i < container.size(); ++i) {
        if (i > 0) {
            joined += glue;
        }
        joined += container[i];
    }
    return joined;
}

template <typename... Ts>
    requires(... && std::is_convertible_v<Ts, std::string_view>)
std::string string_join(std::string_view glue, Ts&... pieces) {
    std::string joined;
    size_t i{};
    (
        [&] {
            if (i++ > 0) {
                joined += glue;
            }
            joined += pieces;
        }(),
        ...);
    return joined;
}

bool string_contains_ci(std::string_view needle, std::string_view haystack);
int string_compare_ci(std::string_view first, std::string_view second);
bool string_compare_less_ci(std::string_view first, std::string_view second);
bool string_equals_ci(std::string_view first, std::string_view second);
bool string_starts_with(std::string_view haystack, std::string_view needle);
bool string_ends_with(std::string_view haystack, std::string_view needle);
void string_split(std::string_view input, char separator, std::function<void(std::string_view token)> cb);
std::vector<std::string> string_split(std::string_view input, char separator);
std::tuple<std::vector<std::string>, std::string> string_split_with_remainder(std::string_view input, char separator);
std::string string_replace(std::string_view input, std::string_view pattern, std::string_view replacement);

std::string regex_replace_callable(const std::string& text, const std::regex& pattern,
                                   std::function<std::string(const std::smatch&)> replacer);

} // namespace ls_gitea_runner::utility
