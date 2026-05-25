#include "runner_task_poller.hpp"

#include <print>
#include <thread>

namespace ls_gitea_runner::gitea {

class GiteaRunnerTaskPoller::Impl final {
public:
    Impl(const GiteaRunnerServiceClient& client, const config::RunnerConfig& config, RunCallbackFn cb)
            : m_client{client}, m_config{config}, m_cb{std::move(cb)} {}

    void run() {
        using namespace std::literals;
        while (true) {
            auto res{wait_for_new_task()};
            if (!res) {
                std::println(std::cerr, "Error: {}", res.error().what());
                std::this_thread::sleep_for(5s);
                continue;
            }
            m_cb(std::move(res->task()));
            std::this_thread::sleep_for(250ms);
        }
    }

private:
    std::expected<::runner::v1::FetchTaskResponse, GenericError> wait_for_new_task() noexcept {
        using namespace std::literals;
        auto fetch_task_response{std::expected<::runner::v1::FetchTaskResponse, GenericError>{}};
        while (true) {
            auto fetch_task_request{::runner::v1::FetchTaskRequest{}};
            fetch_task_response = m_client.get().fetch_task(fetch_task_request);
            if (!fetch_task_response) {
                return std::unexpected{GenericError{"Failed to fetch any new tasks"}};
            }

            if (!fetch_task_response->has_task()) {
                std::this_thread::sleep_for(1s);
                continue;
            }

            break;
        }
        return fetch_task_response;
    }

    std::reference_wrapper<const GiteaRunnerServiceClient> m_client;
    std::reference_wrapper<const config::RunnerConfig> m_config;
    RunCallbackFn m_cb;
};

GiteaRunnerTaskPoller::GiteaRunnerTaskPoller(const GiteaRunnerServiceClient& client, const config::RunnerConfig& config,
                                             RunCallbackFn cb)
        : m_impl{std::make_unique<Impl>(client, config, std::move(cb))} {}

GiteaRunnerTaskPoller::~GiteaRunnerTaskPoller() {}

void GiteaRunnerTaskPoller::run() { m_impl->run(); }

} // namespace ls_gitea_runner::gitea
