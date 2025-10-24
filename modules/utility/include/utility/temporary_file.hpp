#pragma once

#include "filesystem.hpp"
#include "stream.hpp"

#ifdef _WIN32
    #include "string.hpp"
#endif

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>

namespace ls_gitea_runner::fs {

class temporary_file {
public:
    temporary_file() : m_path{fs::temporary_file_path()} { open(); }

    temporary_file(const temporary_file&) = delete;
    temporary_file(temporary_file&& other) noexcept { *this = std::move(other); }
    temporary_file& operator=(const temporary_file&) = delete;

    temporary_file& operator=(temporary_file&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        m_path = std::move(other.m_path);
        m_file = std::move(other.m_file);
        m_valid = std::exchange(other.m_valid, false);
        m_moved = std::exchange(other.m_moved, false);
        return *this;
    }

    ~temporary_file() {
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

    const std::filesystem::path& get_path() const noexcept { return m_path; }

    void move(const std::filesystem::path& new_name) {
        std::filesystem::rename(m_path, new_name);
        m_path = new_name;
        m_moved = true;
    }

    utility::input_stream_ptr create_input_stream() {
        auto ptr{utility::make_std_input_stream(std::move(m_file), std::nullopt, true)};
        return ptr;
    }

    utility::output_stream_ptr create_output_stream() {
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

} // namespace ls_gitea_runner::fs
