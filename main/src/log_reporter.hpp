#pragma once

#include <string>

namespace ls_gitea_runner {

class log_reporter {
public:
    virtual ~log_reporter() = default;
    virtual void add(std::string message) = 0;
    virtual void flush() = 0;
};

} // namespace ls_gitea_runner
