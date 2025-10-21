#include "utility/file_handle.hpp"

#include <utility>

namespace ls_gitea_runner::utility {

file_handle::file_handle(FILE* fp) : m_fp{fp} {}
file_handle::~file_handle() {}
file_handle::file_handle(file_handle&& other) noexcept { *this = std::move(other); }

file_handle& file_handle::operator=(file_handle&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    m_fp = std::exchange(other.m_fp, nullptr);
    return *this;
}

FILE* file_handle::get_native_handle() const noexcept { return m_fp; }

} // namespace ls_gitea_runner::utility
