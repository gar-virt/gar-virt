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

std::string string_from_u8string(std::u8string_view const from);
const std::string_view string_trim_left(const std::string_view s,
                                        const std::set<char> chars = {'\t', '\n', '\f', '\r', ' '});
const std::string_view string_trim_right(const std::string_view s,
                                         const std::set<char> chars = {'\t', '\n', '\f', '\r', ' '});
const std::string_view string_trim(const std::string_view s,
                                   const std::set<char> chars = {'\t', '\n', '\f', '\r', ' '});
void string_trim_right(std::in_place_t, std::string& s, const std::set<char> chars = {'\t', '\n', '\f', '\r', ' '});
void string_trim_right(std::in_place_t, std::string& s, char c);

template <typename Container> std::string string_join(const Container& container, const std::string_view glue) {
    auto joined = std::string{};
    for (auto i = size_t{}; i < container.size(); ++i) {
        if (i > 0) {
            joined += glue;
        }
        joined += container[i];
    }
    return joined;
}

bool string_contains_ci(const std::string_view needle, const std::string_view haystack);
int string_compare_ci(const std::string_view first, const std::string_view second);
bool string_compare_less_ci(const std::string_view first, const std::string_view second);
bool string_equals_ci(const std::string_view first, const std::string_view second);
bool string_starts_with(const std::string_view haystack, const std::string_view needle);
bool string_ends_with(const std::string_view haystack, const std::string_view needle);
void string_split(const std::string_view input, char separator, std::function<void(const std::string_view token)> cb);
std::vector<std::string> string_split(const std::string_view input, char separator);
std::tuple<std::vector<std::string>, std::string> string_split_with_remainder(const std::string_view input,
                                                                              char separator);

std::string regex_replace_callable(const std::string& text, const std::regex& pattern,
                                   std::function<std::string(const std::smatch&)> replacer);

} // namespace ls_gitea_runner::utility
