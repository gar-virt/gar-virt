module;

#include <csignal>
#include <memory>
#include <mutex>

export module utility;

export namespace ls_gitea_runner::utility {

class ShutdownSignal {
    struct State {
        std::mutex mutex;
        bool value{};
    };

public:
    ShutdownSignal() : m_state{std::make_shared<State>()} {}

    void signal() {
        const std::scoped_lock lock{m_state->mutex};
        m_state->value = true;
    }

    bool is_signalled() const {
        const std::scoped_lock lock{m_state->mutex};
        return m_state->value;
    }

    ShutdownSignal install() {
        static ShutdownSignal sig;
        std::once_flag once;
        std::call_once(once, [&] {
            std::signal(SIGINT, +[](int) { sig.signal(); });
            std::signal(SIGTERM, +[](int) { sig.signal(); });
        });
        return sig;
    }

private:
    std::shared_ptr<State> m_state;
};

} // namespace ls_gitea_runner::utility
