#pragma once

#include "memstreambuf.hpp"

#include <istream>

namespace ls_gitea_runner::utility {

template <typename Char, typename Traits = std::char_traits<Char>>
class MemoryInputStream : public std::basic_istream<Char, Traits> {
public:
    MemoryInputStream(const Char* start, const Char* end)
            : std::basic_istream<Char, Traits>{nullptr}, m_buffer{cast(start), cast(end)} {
        this->set_rdbuf(&m_buffer);
        this->clear();
    }

    MemoryInputStream(const Char* start, size_t length) : MemoryInputStream{start, start + length} {}

private:
    static constexpr Char* cast(const Char* p) noexcept {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        return const_cast<typename std::add_pointer<typename std::remove_const<Char>::type>::type>(p);
    }

    MemoryStreambuf<Char, Traits> m_buffer;
};

} // namespace ls_gitea_runner::utility
