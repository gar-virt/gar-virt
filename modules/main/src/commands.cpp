#include "commands.hpp"

#include "config.hpp"
#include "core.hpp"

#include <utility/shutdown_signal.hpp>

#include <expected>
#include <thread>

namespace ls_gitea_runner {

std::expected<void, GenericError> cmd_daemon(config::MainConfig main_config) noexcept {
    using namespace std::chrono_literals;

    auto stop{utility::ShutdownSignal::install()};
    std::vector<std::shared_ptr<TemplateState>> template_states;
    std::vector<std::jthread> threads;

    template_states.reserve(count_max_concurrency(main_config));

    auto shared_main_config{std::make_shared<config::MainConfig>(std::move(main_config))};
    for (auto& backend_config : shared_main_config->backends) {
        auto shared_backend_config{std::make_shared<config::BackendConfig>(std::move(backend_config))};
        for (auto& template_config : shared_backend_config->templates) {
            const auto max_concurrency{template_config.max_concurrency};
            auto shared_template_config{std::make_shared<config::MachineTemplateConfig>(std::move(template_config))};
            auto template_state{template_states.emplace_back(
                TemplateState::create(shared_main_config, shared_backend_config, shared_template_config, stop))};
            for (size_t i{}; i < max_concurrency; ++i) {
                threads.emplace_back([template_state] { template_state->runner_loop(); });
            }
        }
    }

    template_states.clear();

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return {};
}

} // namespace ls_gitea_runner
