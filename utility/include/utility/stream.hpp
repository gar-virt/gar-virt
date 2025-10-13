#pragma once

#include "algorithm.hpp"
#include "memstream.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <istream>
#include <memory>
#include <optional>
#include <streambuf>
#include <string>

namespace ls_gitea_runner::utility {

class ishared_stream_operations {
public:
    virtual ~ishared_stream_operations() = default;

    virtual bool good() const = 0;
    virtual void close() = 0;
    virtual bool is_slow() const = 0;
};

class iinput_stream : public virtual ishared_stream_operations {
public:
    virtual ~iinput_stream() = default;

    virtual bool eof() const = 0;
    virtual intptr_t tell() = 0;
    virtual intptr_t read(void* buffer, uintptr_t count) = 0;
    virtual intptr_t seek(intptr_t count) = 0;
    virtual bool is_size_known() const = 0;
    virtual uintptr_t get_size() const = 0;
};

class ioutput_stream : public virtual ishared_stream_operations {
public:
    virtual ~ioutput_stream() = default;

    virtual void write(const void* buffer, uintptr_t count) = 0;
};

using input_stream_ptr = std::unique_ptr<iinput_stream>;
using output_stream_ptr = std::unique_ptr<ioutput_stream>;

template <typename T> class std_input_stream : public iinput_stream {
public:
    template <typename T_>
    std_input_stream(T_&& stream, std::optional<uintptr_t> size, bool slow)
            : m_stream{std::forward<T_>(stream)}, m_slow{slow}, m_size{size ? size : try_get_stream_size(m_stream)},
              m_initial_position{m_stream.tellg()} {}

    bool good() const override { return m_stream.good(); }
    bool eof() const override { return m_stream.eof(); }
    intptr_t tell() override { return m_stream.tellg(); }

    intptr_t read(void* buffer, uintptr_t count) override {
        uintptr_t amount_available{count};
        if (is_size_known()) {
            const auto pos_diff_s{m_stream.tellg() - m_initial_position};
            if (pos_diff_s < 0) {
                m_stream.setstate(std::ios_base::failbit);
                return 0;
            }
            const auto pos_diff_u{safe_cast_int<uintptr_t>(pos_diff_s)};
            if (pos_diff_u >= *m_size) {
                m_stream.setstate(std::ios_base::eofbit);
                return 0;
            }
            amount_available = *m_size - pos_diff_u;
        }
        if (count > amount_available) {
            count = amount_available;
        }
        m_stream.read(static_cast<char*>(buffer), safe_cast_int<std::streamsize>(count));
        count = m_stream.gcount();
        if (count >= amount_available) {
            m_stream.setstate(std::ios_base::eofbit);
        }
        return count;
    }

    intptr_t seek(intptr_t count) override {
        m_stream.seekg(count, std::ios_base::cur);
        return m_stream.tellg();
    }

    void close() override {
        // Do nothing
    }

    bool is_size_known() const override { return m_size.has_value(); }
    uintptr_t get_size() const override { return m_size.value_or(0); }
    bool is_slow() const override { return m_slow; }

private:
    template <typename Stream> static std::optional<uintptr_t> try_get_stream_size(Stream& stream) {
        const auto begin_pos{stream.tellg()};
        stream.seekg(0, std::ios_base::end);
        const auto end_pos{stream.tellg()};
        const auto diff{end_pos - begin_pos};
        if (diff != 0) {
            stream.seekg(begin_pos, std::ios_base::beg);
        }
        if (diff > 0) {
            return safe_cast_int<uintptr_t>(diff);
        }
        return std::nullopt;
    }

    T m_stream;
    bool m_slow{};
    std::optional<uintptr_t> m_size;
    std::streampos m_initial_position{};
};

template <typename T> class std_output_stream : public ioutput_stream {
public:
    template <typename T_>
    std_output_stream(T_&& stream, bool slow) : m_stream{std::forward<T_>(stream)}, m_slow{slow} {}

    bool good() const override { return m_stream.good(); }

    void write(const void* buffer, uintptr_t count) override {
        m_stream.write(static_cast<const char*>(buffer), safe_cast_int<std::streamsize>(count));
    }

    void close() override {
        // Do nothing
    }

    bool is_slow() const override { return m_slow; }

private:
    T m_stream;
    bool m_slow{};
};

class string_input_stream : public iinput_stream {
public:
    string_input_stream(std::string value) : m_value{std::move(value)}, m_stream{m_value.data(), m_value.size()} {}

    bool good() const override { return m_stream.good(); }
    bool eof() const override { return m_stream.eof(); }
    intptr_t tell() override { return m_stream.tellg(); }

    intptr_t read(void* buffer, uintptr_t count) override {
        m_stream.read(static_cast<char*>(buffer), safe_cast_int<std::streamsize>(count));
        return m_stream.gcount();
    }

    intptr_t seek(intptr_t count) override {
        m_stream.seekg(count, std::ios_base::cur);
        return m_stream.tellg();
    }

    void close() override {
        // Do nothing
    }

    bool is_size_known() const override { return true; }
    uintptr_t get_size() const override { return m_value.size(); }
    bool is_slow() const override { return false; }

private:
    std::string m_value;
    imemstream<char> m_stream;
};

template <typename T> input_stream_ptr make_std_input_stream(T&& stream, std::optional<uintptr_t> size, bool slow) {
    return input_stream_ptr{new std_input_stream<T>(std::forward<T>(stream), size, slow)};
}

template <typename T> output_stream_ptr make_std_output_stream(T&& stream, bool slow) {
    return output_stream_ptr{new std_output_stream<T>(std::forward<T>(stream), slow)};
}

inline input_stream_ptr make_string_input_stream(std::string value) {
    return input_stream_ptr{new string_input_stream(std::move(value))};
}

class file_input_stream : public iinput_stream {
public:
    virtual ~file_input_stream() = default;

    bool good() const override { return m_stream.good(); }
    bool eof() const override { return m_stream.eof(); }
    intptr_t tell() override { return m_stream.tellg(); }

    file_input_stream(const std::filesystem::path& file_path)
            : m_stream{make_file_path_string(file_path)}, m_size{std::filesystem::file_size(file_path)} {}

    intptr_t read(void* buffer, uintptr_t count) override {
        m_stream.read(static_cast<char*>(buffer), safe_cast_int<std::streamsize>(count));
        auto a = m_stream.gcount();
        return a;
    }

    intptr_t seek(intptr_t count) override {
        m_stream.seekg(count, std::ios_base::cur);
        return m_stream.tellg();
    }

    void close() override { m_stream.close(); }
    bool is_size_known() const override { return true; }
    uintptr_t get_size() const override { return m_size; }
    bool is_slow() const override { return true; }

private:
#if defined(_WIN32)
    static std::wstring make_file_path_string(const std::filesystem::path& input) {
        return widen_string(input.string());
    }
#else
    static std::string make_file_path_string(const std::filesystem::path& input) { return input.string(); }
#endif

    mutable std::ifstream m_stream;
    size_t m_size{};
};

class file_output_stream : public ioutput_stream {
public:
    virtual ~file_output_stream() = default;

    bool good() const override { return m_stream.good(); }

    file_output_stream(const std::filesystem::path& file_path) : m_stream{make_file_path_string(file_path)} {}

    void write(const void* buffer, uintptr_t count) override {
        m_stream.write(static_cast<const char*>(buffer), safe_cast_int<std::streamsize>(count));
    }

    void close() override { m_stream.close(); }
    bool is_slow() const override { return true; }

private:
#if defined(_WIN32)
    static std::wstring make_file_path_string(const std::filesystem::path& input) {
        return widen_string(input.string());
    }
#else
    static std::string make_file_path_string(const std::filesystem::path& input) { return input.string(); }
#endif

    mutable std::ofstream m_stream;
};

inline input_stream_ptr make_file_input_stream(const std::filesystem::path& file_path) {
    return input_stream_ptr{new file_input_stream(file_path)};
}

inline output_stream_ptr make_file_output_stream(const std::filesystem::path& file_path) {
    return output_stream_ptr{new file_output_stream(file_path)};
}

class string_streambuf : public std::streambuf {
public:
    string_streambuf(const char* data, size_t length) noexcept {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        auto* p{const_cast<char*>(data)};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        setg(p, p, p + length);
    }

    string_streambuf(const string_streambuf&) = delete;

    string_streambuf(string_streambuf&& other) noexcept { setg(other.eback(), other.gptr(), other.egptr()); }

    string_streambuf& operator=(const string_streambuf&) = delete;
    string_streambuf& operator=(string_streambuf&&) = delete;
    ~string_streambuf() override = default;
};

class string_istream_data {
public:
    string_istream_data(const char* data, size_t length) noexcept : m_buf{data, length} {}

    explicit string_istream_data(const std::string& s) noexcept : string_istream_data{s.data(), s.size()} {}

    explicit string_istream_data(std::string&& s) noexcept
            : m_owned_data{std::move(s)}, m_buf{m_owned_data.data(), m_owned_data.size()} {}

    string_istream_data(const string_istream_data&) = delete;

    string_istream_data(string_istream_data&& other) noexcept
            : m_owned_data{std::move(other.m_owned_data)}, m_buf{std::move(other.m_buf)} {}

    string_istream_data& operator=(const string_istream_data&) = delete;
    string_istream_data& operator=(string_istream_data&& other) = delete;
    ~string_istream_data() = default;

    string_streambuf* get_buf() noexcept { return &m_buf; }

private:
    std::string m_owned_data;
    string_streambuf m_buf;
};

class string_istream : private string_istream_data, public std::istream {
public:
    explicit string_istream(const char* data, size_t length) noexcept
            : string_istream_data{data, length}, std::istream{get_buf()} {}

    explicit string_istream(const std::string& s) noexcept : string_istream{s.data(), s.size()} {}

    explicit string_istream(std::string&& s) noexcept : string_istream_data{std::move(s)}, std::istream{get_buf()} {}

    string_istream(const string_istream&) = delete;

    string_istream(string_istream&& other) noexcept : string_istream_data{std::move(other)}, std::istream{get_buf()} {}

    string_istream& operator=(const string_istream&) = delete;
    string_istream& operator=(string_istream&&) = delete;
    ~string_istream() override = default;
};

inline string_istream make_istream(const char* data, size_t length) noexcept { return string_istream{data, length}; }

template <typename Container> inline string_istream make_istream(Container&& data) noexcept {
    return string_istream{std::forward<Container>(data)};
}

inline std::optional<std::string> read_stream_as_string(utility::input_stream_ptr& stream, size_t max_length) noexcept {
    if (!stream->is_size_known()) {
        return std::nullopt;
    }
    auto content_length{stream->get_size()};
    if (content_length < 1) {
        return std::nullopt;
    }
    if (content_length > max_length) {
        return std::nullopt;
    }
    std::string data;
    data.resize(content_length);
    const auto amount_read{stream->read(&data[0], data.size())};
    data.resize(amount_read);
    return data;
}

inline std::optional<std::string> read_line(utility::iinput_stream& stream) {
    enum class state_t { cr, lf, done };
    auto state{state_t::cr};
    std::string line;
    char c{};
    while (state != state_t::done && stream.read(&c, 1) > 0) {
        switch (state) {
        case state_t::cr:
            if (c == '\r') {
                state = state_t::lf;
                break;
            }
            line += c;
            break;
        case state_t::lf:
            if (c == '\n') {
                state = state_t::done;
                break;
            }
            return std::nullopt;
        case state_t::done:
            break;
        }
    }
    return line;
}

} // namespace ls_gitea_runner::utility
