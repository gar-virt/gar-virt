#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gv::gitea {

struct TaskParcel {
    std::int64_t id{};
    std::vector<std::byte> data;
};

} // namespace gv::gitea
