#include <utility/fs/temporary_file.hpp>

#include <utility/fs/filesystem.hpp>
#include <utility/io/stream.hpp>

#ifdef _WIN32
    #include "string.hpp"
#endif

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>

namespace gv::fs {

class TemporaryFile::Impl {
public:
    Impl() : m_path{fs::temporary_file_path()} { open(); }

    ~Impl() {
        if (!m_valid) {
            return;
        }
        if (m_moved) {
            close();
            return;
        }
        std::filesystem::path new_name{m_path.string() + ".delete"};
        bool renamed{[&] {
            try {
                std::filesystem::rename(m_path, new_name);
                return true;
            } catch (const std::exception&) {
                return false;
            }
        }()};
        close();
        try {
            std::filesystem::remove(renamed ? new_name : m_path);
        } catch (const std::exception&) {
            // Ignore
        }
    }

    Impl(const Impl&) = delete;
    Impl(Impl&& other) noexcept = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&& other) noexcept = delete;

    const std::filesystem::path& get_path() const noexcept { return m_path; }

    void move(const std::filesystem::path& new_name) {
        std::filesystem::rename(m_path, new_name);
        m_path = new_name;
        m_moved = true;
    }

    utility::InputStreamPtr create_input_stream() {
        auto ptr{utility::make_std_input_stream(std::move(m_file), std::nullopt, true)};
        return ptr;
    }

    utility::OutputStreamPtr create_output_stream() {
        auto ptr{utility::make_std_output_stream(std::move(m_file), true)};
        return ptr;
    }

private:
    void open() {
        constexpr auto mode{std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::trunc};
#ifdef _WIN32
        m_file = std::fstream{utility::widen_string(m_path.string()), mode};
#else
        m_file = std::fstream{m_path.string(), mode};
#endif
        m_valid = m_file.good();
    }

    void close() {
        m_file.close();
        m_valid = false;
    }

    std::filesystem::path m_path;
    std::fstream m_file;
    bool m_valid{};
    bool m_moved{};
};

TemporaryFile::TemporaryFile() : m_impl{std::make_unique<Impl>()} {}
TemporaryFile::~TemporaryFile() = default;
TemporaryFile::TemporaryFile(TemporaryFile&& other) noexcept = default;
TemporaryFile& TemporaryFile::operator=(TemporaryFile&& other) noexcept = default;
const std::filesystem::path& TemporaryFile::get_path() const noexcept { return m_impl->get_path(); }
void TemporaryFile::move(const std::filesystem::path& new_name) { m_impl->move(new_name); }
utility::InputStreamPtr TemporaryFile::create_input_stream() { return m_impl->create_input_stream(); }
utility::OutputStreamPtr TemporaryFile::create_output_stream() { return m_impl->create_output_stream(); }

} // namespace gv::fs
