#pragma once

#include <memory>

namespace ls_gitea_runner::utility {

class ShutdownSignal {
public:
    ShutdownSignal();
    void signal() noexcept;
    bool is_signalled() const noexcept;
    static ShutdownSignal install();

private:
    struct State;
    std::shared_ptr<State> m_state;
};

} // namespace ls_gitea_runner::utility
