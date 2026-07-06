#include "commands.hpp"

#include "../config.hpp"
#include "../version.hpp"

#include <gitea/admin_service_client.hpp>
#include <gitea/runner.hpp>
#include <gitea/runner_service_client.hpp>
#include <runner/v1/messages.pb.h>
#include <utility/algorithm.hpp>
#include <utility/defer.hpp>
#include <utility/log/global_logger.hpp>
#include <utility/string.hpp>
#include <utility/thread_pool_executor.hpp>
#include <virt/machine_manager_factory_selector.hpp>
#include <virt/machine_pool.hpp>

#include <boost/json.hpp>
#include <boost/url.hpp>

#include <atomic>
#include <csignal>
#include <expected>
#include <format>
#include <optional>
#include <print>
#include <string>

#define LOG_SELECT(true_level, false_level, cond, ...)                                                                 \
    global_logger().log((cond) ? (utility::LogLevel::true_level) : (utility::LogLevel::false_level), __VA_ARGS__);

namespace ls_gitea_runner {

struct Injectables {
    std::string runner_state_json;
    std::string runner_config_yaml;
    std::vector<std::byte> encoded_task;

    static std::expected<Injectables, GenericError>
    generate(const ::runner::v1::Task& task, const gitea::Runner& runner, const config::RunnerConfig& config) {
        auto encode_payload{gitea::encode_payload(task)};
        if (!encode_payload) {
            return std::unexpected{encode_payload.error()};
        }
        boost::json::array labels;
        for (auto label : runner.labels()) {
            labels.emplace_back(label);
        }
        return Injectables{
            .runner_state_json = boost::json::serialize(boost::json::object{
                {"id", runner.id()},
                {"uuid", runner.credentials().uuid},
                {"token", runner.credentials().token},
                {"address", config.forge.uri},
                {"labels", labels},
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

std::expected<std::vector<std::string>, GenericError> make_ping_command(const std::string& target_os,
                                                                        const std::string& host) {
    std::vector<std::string> cmd = {"ping"};
    if (utility::string_compare_ci(target_os, "linux")) {
        // Count
        cmd.emplace_back("-c");
        cmd.emplace_back("1");
        // Timeout in seconds
        cmd.emplace_back("-W");
        cmd.emplace_back("2");
    } else if (utility::string_compare_ci(target_os, "macos")) {
        // Count
        cmd.emplace_back("-c");
        cmd.emplace_back("1");
        // Timeout in milliseconds
        cmd.emplace_back("-W");
        cmd.emplace_back("2000");
    } else if (utility::string_compare_ci(target_os, "windows")) {
        // Count
        cmd.emplace_back("-n");
        cmd.emplace_back("1");
        // Timeout in seconds
        cmd.emplace_back("-w");
        cmd.emplace_back("2");
    } else {
        return std::unexpected{GenericError{std::format("Ping command not implemented for target OS {}", target_os)}};
    }
    cmd.push_back(host);
    return cmd;
}

std::expected<void, GenericError> wait_until_gitea_instance_available(Machine& machine, const std::string& instance_url,
                                                                      std::chrono::seconds timeout) {
    using namespace std::literals;
    const auto parsed_instance_url{boost::urls::parse_uri(instance_url)};
    const auto& host{parsed_instance_url->host()};

    const auto cmd{make_ping_command(machine.info().os, host)};
    if (!cmd) {
        return std::unexpected{cmd.error()};
    }

    utility::SpawnResult spawn_res;
    auto start_time{std::chrono::steady_clock::now()};
    while (start_time - std::chrono::steady_clock::now() < timeout) {
        auto res{machine.shell_exec(*cmd)};
        if (!res) {
            return std::unexpected{
                GenericError{std::format("Failed to execute ping command for host {} in machine {}: {}", host,
                                         machine.get_id(), res.error().what())}};
        }
        spawn_res = *res;
        if (spawn_res.exit_code == 0) {
            global_logger().verbose("Ping command completed successfully in machine {}", machine.get_id());
            return {};
        }
        std::this_thread::sleep_for(2s);
    }

    return std::unexpected{GenericError{std::format("Ping timeout for host {} in machine {}, with output: {}", host,
                                                    machine.get_id(), spawn_res.output)}};
}

std::expected<std::unique_ptr<Machine>, GenericError> spawn_machine(const config::RunnerConfig& config) noexcept {
    using namespace std::literals;

    const auto& pool_config{config.machine_pool};
    const auto& provider{pool_config.provider};
    const auto& template_config{pool_config.machine_template};

    auto machine_manager_factory_res{MachineManagerFactorySelector::get_factory(provider)};
    if (!machine_manager_factory_res) {
        return std::unexpected{machine_manager_factory_res.error()};
    }

    auto machine_manager_factory{std::move(*machine_manager_factory_res)};
    auto machine_manager{machine_manager_factory->create()};

    global_logger().verbose("Spawning new {} machine: os = {}; arch = {}", provider, template_config.os,
                            template_config.arch);

    auto machine_res{machine_manager->spawn(
        Machine::Info{
            .os = template_config.os,
            .arch = template_config.arch,
        },
        pool_config.details_as_yaml, template_config.details_as_yaml, config.config_base_dir)};

    if (!machine_res) {
        return std::unexpected{machine_res.error()};
    }

    auto machine{*std::move(machine_res)};

    global_logger().verbose("Spawning new {} machine: os = {}; arch = {}; id = {}", provider, template_config.os,
                            template_config.arch, machine->get_id());

    global_logger().verbose("Waiting for machine {} guest agent.", machine->get_id());
    if (auto res{machine->wait_for_guest_agent(120s)}; !res) {
        return std::unexpected{res.error()};
    }

    global_logger().verbose("Guest agent is available in machine {}", machine->get_id());

    global_logger().verbose("Waiting for machine {} network.", machine->get_id());
    if (!wait_until_gitea_instance_available(*machine, config.forge.uri, 120s)) {
        return std::unexpected{GenericError{std::format(
            "Timed out while waiting for networking to become available in machine {}.", machine->get_id())}};
    }

    global_logger().verbose("Networking is available in machine {}.", machine->get_id());

    return machine;
}

std::expected<void, GenericError> execute_task_in_machine(const ::runner::v1::Task& task, const gitea::Runner& runner,
                                                          const config::RunnerConfig& config,
                                                          Machine& machine) noexcept {
    using namespace std::literals;
    const auto id{task.id()};

    global_logger().verbose("Preparing environment for machine {}.", machine.get_id());

    return Injectables::generate(task, runner, config)
        .and_then([&](auto res) { return inject_runner_files(machine, std::move(res)); })
        .and_then([&] {
            global_logger().verbose("Executing task #{} in machine {}.", task.id(), machine.get_id());
            return machine
                .shell_exec({config.machine_pool.machine_template.runner_exe_path, "run-task", "--config",
                             "/tmp/runner_config.yml", "--task", "/tmp/runner_task"})
                .and_then([&](auto res) -> std::expected<void, GenericError> {
                    LOG_SELECT(verbose, error, res.exit_code == 0,
                               "Task #{} execution exited with code {} and output: {}", task.id(), res.exit_code,
                               res.output);
                    return {};
                });
        });
}

void update_task_on_error(const ::runner::v1::Task& task, const gitea::GiteaRunnerServiceClient& client) {
    ::runner::v1::UpdateTaskRequest update_req;
    auto& task_state{*update_req.mutable_state()};
    task_state.set_id(task.id());
    task_state.set_result(::runner::v1::RESULT_FAILURE);
    std::ignore = client.update_task(update_req);
}

std::expected<void, GenericError> fetch_task_loop(std::shared_ptr<gitea::AdminServiceClient> admin,
                                                  MachinePool& machine_pool, utility::ThreadPoolExecutor& task_executor,
                                                  const config::RunnerConfig& config, std::atomic_bool& stop,
                                                  std::counting_semaphore<>& capacity_guard) noexcept {
    using namespace std::literals;

    while (!stop && !capacity_guard.try_acquire()) {
        std::this_thread::sleep_for(1s);
    }

    if (stop) {
        return std::unexpected{GenericError{"Fetch task loop is shutting down"}};
    }

    gitea::RunnerOptions runner_options{
        .instance_url = config.forge.uri,
        .name = config.name,
        .labels = config.machine_pool.machine_template.labels,
        .version = std::string{runner_version},
    };

    auto runner_res{gitea::Runner::connect(runner_options, std::move(admin))};
    if (!runner_res) {
        capacity_guard.release();
        return std::unexpected{runner_res.error()};
    }
    auto runner{*std::move(runner_res)};

    std::optional<::runner::v1::Task> task;
    while (!stop && !task.has_value()) {
        auto res{runner.fetch_task()};
        if (!res) {
            capacity_guard.release();
            return std::unexpected{res.error()};
        }

        if (!res->has_task()) {
            std::this_thread::sleep_for(1s);
            continue;
        }

        task = res->task();
    }

    if (task.has_value()) {
        global_logger().verbose("Runner {} fetched task with ID {}.", runner.id(), task->id());
        task_executor.put(
            [&capacity_guard](auto config, auto* machine_pool, auto task, auto runner) {
                // FIXME: configurable timeout
                auto machine_res{machine_pool->acquire(120s)};
                if (!machine_res) {
                    global_logger().error("Failed to acquire machine from pool: {}", machine_res.error().what());
                    update_task_on_error(task, runner.client());
                    capacity_guard.release();
                    return;
                }
                auto machine{*std::move(machine_res)};
                std::ignore = execute_task_in_machine(task, runner, config, *machine)
                                  .or_else([&](auto err) -> std::expected<void, GenericError> {
                                      global_logger().error("Error during task execution: {}", err.what());
                                      update_task_on_error(task, runner.client());
                                      return {};
                                  });
                machine_pool->release(machine);
                capacity_guard.release();
            },
            config, &machine_pool, *std::move(task), std::move(runner));
    } else {
        capacity_guard.release();
    }

    return {};
}

void runner_loop(const config::RunnerConfig& config, std::atomic_bool& stop) {
    using namespace std::literals;
    auto admin{std::make_shared<gitea::AdminServiceClient>(config.forge.uri, config.forge.token)};
    std::counting_semaphore capacity_guard{utility::safe_cast_int<ptrdiff_t>(config.machine_pool.capacity)};
    MachinePool machine_pool{config.machine_pool.capacity, [&] { return spawn_machine(config); }};
    machine_pool.set_stats_callback([&](auto stats) {
        global_logger().verbose("{} machine pool stats: {} active; {} idle; {} warmup; {} capacity",
                                config.machine_pool.provider, stats.active, stats.idle, stats.warming,
                                config.machine_pool.capacity);
    });
    utility::ThreadPoolExecutor task_executor{config.machine_pool.capacity};
    machine_pool.start();
    while (!stop) {
        auto res{fetch_task_loop(admin, machine_pool, task_executor, config, stop, capacity_guard)};
        if (!res) {
            global_logger().error("Error in run loop iteration: {}", res.error().what());
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

std::expected<void, GenericError> cmd_daemon(const config::RunnerConfig& config) noexcept {
    using namespace std::chrono_literals;
    auto& shutdown{install_shutdown_signal_handler()};
    std::vector<std::jthread> runner_threads;
    // TODO: Create one polling thread per runner - currently just one
    runner_threads.emplace_back([&] { runner_loop(config, shutdown); });
    return {};
}

} // namespace ls_gitea_runner
