#pragma once

#include <cstdio>

namespace ls_gitea_runner::utility {

class FileHandle {
public:
    FileHandle(FILE* fp);
    ~FileHandle();
    FileHandle(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) noexcept;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle& operator=(FileHandle&& other) noexcept;
    FILE* get_native_handle() const noexcept;

private:
    FILE* m_fp{};
};

} // namespace ls_gitea_runner::utility
