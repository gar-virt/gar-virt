#pragma once

#include <memory>

namespace gv::utility {

class ShutdownSignal {
public:
    ShutdownSignal();
    void signal();
    bool is_signalled() const;
    static ShutdownSignal install();

private:
    struct State;
    std::shared_ptr<State> m_state;
};

} // namespace gv::utility
