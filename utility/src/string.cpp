#include "utility/string.hpp"

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
    #include <stdexcept>
#endif

#include <algorithm>
#include <iostream>

namespace utility {

#ifdef _WIN32
std::wstring widen(const std::string& narrowString) {
    auto cp{CP_UTF8};
    auto flags{MB_ERR_INVALID_CHARS};
    auto narrowStringC{narrowString.c_str()};
    auto narrowStringLength{narrowString.size()};
    auto requiredLength{::MultiByteToWideChar(cp, flags, narrowStringC, narrowStringLength, nullptr, 0)};
    if (requiredLength > 0) {
        std::wstring wideString(requiredLength, '\0');
        if (::MultiByteToWideChar(cp, flags, narrowStringC, narrowStringLength, wideString.data(), wideString.size()) > 0) {
            return wideString;
        }
    }
    throw std::runtime_error{"Failed to convert string from UTF-8 to UTF-16"};
}

std::string narrow(const std::wstring& wideString) {
    auto cp{CP_UTF8};
    auto flags{WC_ERR_INVALID_CHARS};
    auto wideStringC{wideString.c_str()};
    auto wideStringLength{wideString.size()};
    auto requiredLength{::WideCharToMultiByte(cp, flags, wideStringC, wideStringLength, nullptr, 0, nullptr, nullptr)};
    if (requiredLength > 0) {
        std::string narrowString(requiredLength, '\0');
        if (::WideCharToMultiByte(cp, flags, wideStringC, wideStringLength, narrowString.data(), narrowString.size(), nullptr, nullptr) > 0) {
            return narrowString;
        }
    }
    throw std::runtime_error{"Failed to convert string from UTF-8 to UTF-16"};
}
#endif

std::string charStringFromChar8String(const std::u8string_view from) {
    std::string result;
    result.reserve(from.size());
    for (auto c : from) {
        result.push_back(static_cast<char>(c));
    }
    return result;
}

void trimRight(std::string& s, char c) {
    auto length{s.size()};
    for (auto it{s.rbegin()}; it != s.rend(); ++it) {
        if (*it == c) {
            --length;
        }
    }
    if (length != s.size()) {
        s.resize(length);
    }
}

bool ciStringContains(const std::string_view needle, const std::string_view haystack) {
    if (needle.empty() && haystack.empty()) {
        return true;
    }
    if (haystack.empty()) {
        return false;
    }
    auto match{std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), [](auto a, auto b) {
        return std::toupper(a) == std::toupper(b);
    })};
    return match != haystack.end();
}

int ciStringCompare(const std::string_view first, const std::string_view second) {
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

bool ciStringCompareLess(const std::string_view first, const std::string_view second) {
    return std::lexicographical_compare(first.begin(), first.end(), second.begin(), second.end(), [](auto a, auto b) {
        return std::toupper(a) < std::toupper(b);
    });
}

bool ciStringEquals(const std::string_view first, const std::string_view second) {
    return std::equal(first.begin(), first.end(), second.begin(), second.end(), [](auto a, auto b) {
        return std::toupper(a) == std::toupper(b);
    });
}

bool ciStringStartsWith(const std::string_view haystack, const std::string_view needle) {
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

bool ciStringEndsWith(const std::string_view haystack, const std::string_view needle) {
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

}
