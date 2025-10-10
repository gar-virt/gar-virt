#pragma once

#include <string>
#include <string_view>

namespace utility {

#ifdef _WIN32
std::wstring widen(const std::string& narrowString);
std::string narrow(const std::wstring& wideString);
#endif

std::string charStringFromChar8String(const std::u8string_view from);
void trimRight(std::string& s, char c);

template<typename Container>
std::string stringJoin(const Container& container, const std::string_view glue) {
    std::string joined;
    for (size_t i{}; i < container.size(); ++i) {
        if (i > 0) {
            joined += glue;
        }
        joined += container[i];
    }
    return joined;
}

bool ciStringContains(const std::string_view needle, const std::string_view haystack);
int ciStringCompare(const std::string_view first, const std::string_view second);
bool ciStringCompareLess(const std::string_view first, const std::string_view second);
bool ciStringEquals(const std::string_view first, const std::string_view second);
bool ciStringStartsWith(const std::string_view haystack, const std::string_view needle);
bool ciStringEndsWith(const std::string_view haystack, const std::string_view needle);

}
