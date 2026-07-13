#pragma once

#include <utility/log/log.hpp>

namespace ls_gitea_runner::utility {

class StdOutLogger final : public Logger {
public:
    StdOutLogger() noexcept;

private:
    void print_impl(const LogRequest& req) noexcept override;

    bool m_enable_color{};
};

} // namespace ls_gitea_runner::utility
