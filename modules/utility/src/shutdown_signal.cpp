#include <utility/shutdown_signal.hpp>

#include <csignal>
#include <memory>
#include <mutex>

namespace ls_gitea_runner::utility {

struct ShutdownSignal::State {
    std::mutex mutex;
    bool value{};
};

ShutdownSignal::ShutdownSignal() : m_state{std::make_shared<State>()} {}

void ShutdownSignal::signal() noexcept {
    std::scoped_lock lock{m_state->mutex};
    m_state->value = true;
}

bool ShutdownSignal::is_signalled() const noexcept {
    std::scoped_lock lock{m_state->mutex};
    return m_state->value;
}

ShutdownSignal ShutdownSignal::install() {
    static ShutdownSignal sig;
    std::once_flag once;
    std::call_once(once, [&] {
        std::signal(SIGINT, +[](int) { sig.signal(); });
        std::signal(SIGTERM, +[](int) { sig.signal(); });
    });
    return sig;
}

} // namespace ls_gitea_runner::utility
