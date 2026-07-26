#pragma once

#include <utility/log/log.hpp>

namespace gv::utility {

class StdOutLogger final : public Logger {
public:
    StdOutLogger() noexcept;

private:
    void print_impl(const LogRequest& req) override;

    bool m_enable_color{};
};

} // namespace gv::utility
