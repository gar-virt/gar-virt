#pragma once

#include <cstdio>

namespace ls_gitea_runner::utility {

class file_handle {
public:
    file_handle(FILE* fp);
    ~file_handle();
    file_handle(const file_handle&) = delete;
    file_handle(file_handle&& other) noexcept;
    file_handle& operator=(const file_handle&) = delete;
    file_handle& operator=(file_handle&& other) noexcept;
    FILE* get_native_handle() const noexcept;

private:
    FILE* m_fp{};
};

} // namespace ls_gitea_runner::utility
