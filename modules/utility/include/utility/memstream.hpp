#pragma once

#include "memstreambuf.hpp"

#include <istream>

namespace ls_gitea_runner::utility {

template <typename Char, typename Traits = std::char_traits<Char>>
class imemstream : public std::basic_istream<Char, Traits> {
public:
    imemstream(const Char* start, const Char* end)
            : std::basic_istream<Char, Traits>{nullptr}, m_buffer{cast(start), cast(end)} {
        this->set_rdbuf(&m_buffer);
        this->clear();
    }

    imemstream(const Char* start, size_t length) : imemstream{start, start + length} {}

private:
    static constexpr Char* cast(const Char* p) noexcept {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        return const_cast<typename std::add_pointer<typename std::remove_const<Char>::type>::type>(p);
    }

    memstreambuf<Char, Traits> m_buffer;
};

} // namespace ls_gitea_runner::utility
