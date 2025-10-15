#include "gitea_runner_task_poller.hpp"

#include <print>
#include <thread>

namespace ls_gitea_runner::gitea {

class gitea_runner_task_poller::impl final {
public:
    impl(const gitea_runner_service_client& client, const config::runner_config& config, run_callback_fn cb)
            : m_client{client}, m_config{config}, m_cb{std::move(cb)} {}

    void run() {
        using namespace std::literals;
        while (true) {
            auto res{wait_for_new_task()};
            if (!res) {
                std::println(std::cerr, "Error: {}", res.error().what());
                std::this_thread::sleep_for(5s);
            }
            m_cb(std::move(res->task()));
            std::this_thread::sleep_for(250ms);
        }
    }

private:
    std::expected<::runner::v1::FetchTaskResponse, generic_error> wait_for_new_task() noexcept {
        using namespace std::literals;
        auto fetch_task_response{std::expected<::runner::v1::FetchTaskResponse, generic_error>{}};
        while (true) {
            auto fetch_task_request{::runner::v1::FetchTaskRequest{}};
            fetch_task_response = m_client.get().fetch_task(fetch_task_request);
            if (!fetch_task_response) {
                return std::unexpected{generic_error{"Failed to fetch any new tasks"}};
            }

            if (!fetch_task_response->has_task()) {
                std::this_thread::sleep_for(1s);
                continue;
            }

            break;
        }
        return fetch_task_response;
    }

    std::reference_wrapper<const gitea_runner_service_client> m_client;
    std::reference_wrapper<const config::runner_config> m_config;
    run_callback_fn m_cb;
};

gitea_runner_task_poller::gitea_runner_task_poller(const gitea_runner_service_client& client,
                                                   const config::runner_config& config, run_callback_fn cb)
        : m_impl{std::make_unique<impl>(client, config, std::move(cb))} {}

gitea_runner_task_poller::~gitea_runner_task_poller() {}

void gitea_runner_task_poller::run() { m_impl->run(); }

} // namespace ls_gitea_runner::gitea
