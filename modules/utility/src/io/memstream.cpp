export module utility.io:memstream;

import std;
import std.compat;

namespace ls_gitea_runner::utility {

export template <typename Char, typename Traits = std::char_traits<Char>>
class MemoryStreambuf : public std::basic_streambuf<Char, Traits> {
public:
    MemoryStreambuf(Char* start, Char* end) { this->setg(start, start, end); }

    std::streambuf::pos_type seekoff(std::streambuf::off_type offset, std::ios_base::seekdir dir,
                                     std::ios_base::openmode mode) override {
        if ((mode & std::ios_base::in) == 0) {
            // Not supported
            return std::streambuf::pos_type{std::streambuf::off_type{-1}};
        }
        if (dir == std::ios_base::beg) {
            return seekpos(offset, mode);
        }
        if (dir == std::ios_base::end) {
            auto pos{static_cast<std::streambuf::pos_type>((this->egptr() - this->eback()) + offset)};
            return seekpos(pos, mode);
        }
        if (dir == std::ios_base::cur) {
            auto pos{static_cast<std::streambuf::pos_type>((this->gptr() - this->eback()) + offset)};
            return seekpos(pos, mode);
        }
        // Should never happen
        return std::streambuf::pos_type{std::streambuf::off_type{-1}};
    }

    std::streambuf::pos_type seekpos(std::streambuf::pos_type pos, std::ios_base::openmode mode) override {
        if ((mode & std::ios_base::in) == 0) {
            // Not supported
            return std::streambuf::pos_type{std::streambuf::off_type{-1}};
        }
        if (this->eback() + pos < this->eback() || this->eback() + pos >= this->egptr()) {
            // Out of range
            return std::streambuf::pos_type{std::streambuf::off_type{-1}};
        }
        this->setg(this->eback(), this->eback() + pos, this->egptr());
        return static_cast<std::streambuf::pos_type>(this->gptr() - this->eback());
    }
};

export template <typename Char, typename Traits = std::char_traits<Char>>
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
