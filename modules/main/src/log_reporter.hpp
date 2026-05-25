#pragma once

#include <string>

namespace ls_gitea_runner {

class LogReporter {
public:
    virtual ~LogReporter() = default;
    virtual void add(std::string message) = 0;
    virtual void flush() = 0;
};

} // namespace ls_gitea_runner
