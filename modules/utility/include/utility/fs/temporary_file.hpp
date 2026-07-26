#pragma once

#include <utility/fs/filesystem.hpp>
#include <utility/io/stream.hpp>

#include <filesystem>

namespace gv::fs {

class TemporaryFile {
public:
    TemporaryFile();
    ~TemporaryFile();
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile(TemporaryFile&& other) noexcept;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    TemporaryFile& operator=(TemporaryFile&& other) noexcept;
    const std::filesystem::path& get_path() const noexcept;
    void move(const std::filesystem::path& new_name);
    utility::InputStreamPtr create_input_stream();
    utility::OutputStreamPtr create_output_stream();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace gv::fs
