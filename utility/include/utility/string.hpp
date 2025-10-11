#pragma once

#include <string>
#include <string_view>

namespace utility {

#ifdef _WIN32
std::wstring widen(const std::string& narrowString);
std::string narrow(const std::wstring& wideString);
#endif

std::string string_from_u8string(std::u8string_view const from);
void string_trim_right(std::string& s, char c);

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

} // namespace utility
