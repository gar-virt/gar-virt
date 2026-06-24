#include "commands.hpp"

#include "ping/v1/messages.pb.h"
#include "runner/v1/messages.pb.h"

#include "../config.hpp"
#include "../version.hpp"

#include "gitea/runner_service_client.hpp"
#include "utility/thread_pool_executor.hpp"
#include "virt/machine_manager_factory_selector.hpp"

#include <boost/json.hpp>

#include <atomic>
#include <csignal>
#include <cstring>
#include <expected>
#include <format>
#include <iostream>
#include <print>
#include <string>

namespace ls_gitea_runner {

std::expected<::ping::v1::PingResponse, GenericError> ping(const gitea::GiteaRunnerServiceClient& client,
                                                           const config::RunnerConfig& config) {
    auto ping_request{::ping::v1::PingRequest{}};
    ping_request.set_data(config.name);
    auto ping_response{client.ping(ping_request)};
    if (!ping_response) {
        return std::unexpected{GenericError{std::format("Failed to send ping: {}", ping_response.error().what())}};
    }
    return *ping_response;
}

std::expected<::runner::v1::RegisterResponse, GenericError> register_(const gitea::GiteaRunnerServiceClient& client,
                                                                      const config::RunnerConfig& config) {
    auto reqister_request{::runner::v1::RegisterRequest{}};
    reqister_request.set_name(config.name);
    reqister_request.set_token(config.token);
    reqister_request.set_version(std::string{runner_version});
    for (auto& label : config.get_label_names()) {
        reqister_request.add_labels(label);
    }
    reqister_request.set_ephemeral(true);
    auto register_response{client.register_(reqister_request)};
    if (!register_response) {
        return std::unexpected{
            GenericError{std::format("Failed to register runner: {}", register_response.error().what())}};
    }
    return *register_response;
}

std::expected<::runner::v1::DeclareResponse, GenericError> declare(const gitea::GiteaRunnerServiceClient& client,
                                                                   const config::RunnerConfig& config) {
    auto declare_request{::runner::v1::DeclareRequest{}};
    declare_request.set_version(std::string{runner_version});
    for (auto& label : config.get_label_names()) {
        declare_request.add_labels(label);
    }
    auto declare_response{client.declare(declare_request)};
    if (!declare_response) {
        return std::unexpected{
            GenericError{std::format("Failed to declare runner: {}", declare_response.error().what())}};
    }
    return declare_response;
}

std::expected<::runner::v1::FetchTaskResponse, GenericError> fetch_task(const gitea::GiteaRunnerServiceClient& client,
                                                                        const config::RunnerConfig& config) {
    auto fetch_task_request{::runner::v1::FetchTaskRequest{}};
    auto fetch_task_response = client.fetch_task(fetch_task_request);
    if (!fetch_task_response) {
        return std::unexpected{GenericError{"Failed to fetch any new tasks"}};
    }
    return *fetch_task_response;
}

struct Injectables {
    std::string runner_state_json;
    std::string runner_config_yaml;
    std::vector<std::byte> encoded_task;

    static std::expected<Injectables, GenericError>
    generate(const ::runner::v1::Task& task, const ::runner::v1::Runner& runner, const config::RunnerConfig& config) {
        auto encode_payload{gitea::encode_payload(task)};
        if (!encode_payload) {
            return std::unexpected{encode_payload.error()};
        }
        return Injectables{
            .runner_state_json = boost::json::serialize(boost::json::object{
                {"id", runner.id()},
                {"uuid", runner.uuid()},
                {"token", runner.token()},
                {"address", config.instance_url},
                {"labels", config.labels},
                {"ephemeral", true},
            }),
            .runner_config_yaml = std::format(R"(runner:
  file: /tmp/.runner
)"),
            .encoded_task = *encode_payload,
        };
    }
};

std::expected<void, GenericError> inject_runner_files(Machine& machine, Injectables injectables) {
    return machine
        .write_file("/tmp/runner_config.yml",
                    {reinterpret_cast<const std::byte*>(injectables.runner_config_yaml.data()),
                     injectables.runner_config_yaml.size()})
        .and_then([&] {
            return machine.write_file("/tmp/.runner",
                                      {reinterpret_cast<const std::byte*>(injectables.runner_state_json.data()),
                                       injectables.runner_state_json.size()});
        })
        .and_then([&] { return machine.write_file("/tmp/runner_task", injectables.encoded_task); });
}

std::expected<void, GenericError> process_task(const ::runner::v1::Task& task, const ::runner::v1::Runner& runner,
                                               const config::RunnerConfig& config) {
    using namespace std::literals;
    const auto id{task.id()};

    const auto& machine_config{config.machine_pool};
    const auto& machine_provider{machine_config.provider};

    auto machine_manager_factory_res{MachineManagerFactorySelector::get_factory(machine_provider)};
    if (!machine_manager_factory_res) {
        return std::unexpected{machine_manager_factory_res.error()};
    }

    auto machine_manager_factory{std::move(*machine_manager_factory_res)};
    auto machine_manager{machine_manager_factory->create()};
    auto machine_res{machine_manager->spawn(
        Machine::Info{
            .os = machine_config.os,
            .arch = machine_config.arch,
            .temp_dir = machine_config.temp_dir,
        },
        MachinePoolDetails{config.config_base_dir, machine_config.details_as_json})};

    if (!machine_res) {
        return std::unexpected{machine_res.error()};
    }

    auto machine{*std::move(machine_res)};
    if (!machine->wait_until_ready(120s)) {
        return std::unexpected{
            GenericError{std::format("Timed out while waiting for machine to become ready: {}", machine->get_id())}};
    }

    return Injectables::generate(task, runner, config)
        .and_then([&](auto res) { return inject_runner_files(*machine, std::move(res)); })
        .and_then([&] {
            return machine
                ->shell_exec({machine_config.runner_exe_path, "run-task", "--config", "/tmp/runner_config.yml",
                              "--task", "/tmp/runner_task"})
                .and_then([](auto res) -> std::expected<void, GenericError> {
                    if (res.exit_code != 0) {
                        return std::unexpected{GenericError{std::format("Gitea Runner returned {}", res.exit_code)}};
                    }
                    return {};
                });
        });
}

template <typename Callable>
std::expected<void, GenericError> update_task_on_error(const ::runner::v1::Task& task,
                                                       const gitea::GiteaRunnerServiceClient& client, Callable fn) {
    return fn().or_else([&](auto err) -> std::expected<void, GenericError> {
        ::runner::v1::UpdateTaskRequest update_req;
        auto& task_state{*update_req.mutable_state()};
        task_state.set_id(task.id());
        task_state.set_result(::runner::v1::RESULT_FAILURE);
        std::ignore = client.update_task(update_req);
        return std::unexpected{std::move(err)};
    });
}

std::expected<void, GenericError> run_loop_iterate(const config::RunnerConfig& config, std::atomic_bool& stop) {
    using namespace std::literals;
    gitea::GiteaRunnerServiceClient client{config.instance_url};

    auto ping_res{ping(client, config)};
    if (!ping_res) {
        return std::unexpected{ping_res.error()};
    }

    auto register_res{register_(client, config)};
    if (!register_res) {
        return std::unexpected{register_res.error()};
    }

    auto& runner{register_res->runner()};
    client.set_credentials(gitea::GiteaRunnerCredentials{.uuid = runner.uuid(), .token = runner.token()});

    auto declare_res{declare(client, config)};
    if (!declare_res) {
        return std::unexpected{declare_res.error()};
    }

    while (!stop) {
        auto res{fetch_task(client, config)};
        if (!res) {
            return std::unexpected{res.error()};
        }

        if (!res->has_task()) {
            std::this_thread::sleep_for(1s);
            continue;
        }

        const auto& task{res->task()};

        return update_task_on_error(task, client, [&] { return process_task(task, runner, config); });
    }

    return {};
}

void run_loop(const config::RunnerConfig& config, std::atomic_bool& stop) {
    using namespace std::literals;
    while (!stop) {
        auto res{run_loop_iterate(config, stop)};
        if (!res) {
            std::println(std::cerr, "Error: {}", res.error().what());
            std::this_thread::sleep_for(5s);
            continue;
        }
        std::this_thread::sleep_for(250ms);
    }
}

std::atomic_bool& install_shutdown_signal_handler() {
    static std::atomic_bool flag{};
    std::signal(SIGINT, +[](int) { flag = true; });
    std::signal(SIGTERM, +[](int) { flag = true; });
    return flag;
}

std::expected<void, GenericError> cmd_daemon(config::RunnerConfig config) noexcept {
    auto& shutdown{install_shutdown_signal_handler()};
    utility::ThreadPoolExecutor executor{config.machine_pool.capacity};
    executor.put([&] { run_loop(config, shutdown); });
    return {};
}

} // namespace ls_gitea_runner
