#include "utility/file_handle.hpp"

#include <utility>

namespace ls_gitea_runner::utility {

FileHandle::FileHandle(FILE* fp) : m_fp{fp} {}
FileHandle::~FileHandle() {}
FileHandle::FileHandle(FileHandle&& other) noexcept { *this = std::move(other); }

FileHandle& FileHandle::operator=(FileHandle&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    m_fp = std::exchange(other.m_fp, nullptr);
    return *this;
}

FILE* FileHandle::get_native_handle() const noexcept { return m_fp; }

} // namespace ls_gitea_runner::utility
