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
#include <utility/shutdown_signal.hpp>
#include <utility/string.hpp>
#include <utility/thread_pool_executor.hpp>
#include <virt/machine_manager_factory_selector.hpp>
#include <virt/machine_pool.hpp>

#include <boost/json.hpp>
#include <boost/url.hpp>

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

    static std::expected<Injectables, GenericError> generate(const ::runner::v1::Task& task,
                                                             const gitea::Runner& runner) {
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
                {"address", runner.forge_uri()},
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
            return {};
        }
        std::this_thread::sleep_for(2s);
    }

    return std::unexpected{GenericError{std::format("Ping timeout for host {} in machine {}, with output: {}", host,
                                                    machine.get_id(), spawn_res.output)}};
}

std::expected<std::unique_ptr<Machine>, GenericError>
spawn_machine(const config::MainConfig& main_config, const config::BackendConfig& backend_config,
              const config::MachineTemplateConfig& template_config) noexcept {
    using namespace std::literals;

    const auto& backend_type{backend_config.type};

    auto machine_manager_factory_res{MachineManagerFactorySelector::get_factory(backend_type)};
    if (!machine_manager_factory_res) {
        return std::unexpected{machine_manager_factory_res.error()};
    }

    auto machine_manager_factory{std::move(*machine_manager_factory_res)};
    auto machine_manager{machine_manager_factory->create()};

    global_logger().verbose("Spawning new {} machine: os = {}; arch = {}", backend_type, template_config.os,
                            template_config.arch);

    auto machine_res{machine_manager->spawn(
        Machine::Info{
            .os = template_config.os,
            .arch = template_config.arch,
        },
        backend_config.raw_details, template_config.raw_details, main_config.base_dir)};

    if (!machine_res) {
        return std::unexpected{machine_res.error()};
    }

    auto machine{*std::move(machine_res)};

    global_logger().verbose("Spawned new {} machine: os = {}; arch = {}; id = {}", backend_type, template_config.os,
                            template_config.arch, machine->get_id());

    global_logger().verbose("Waiting for machine {} guest agent.", machine->get_id());
    if (auto res{machine->wait_for_guest_agent(120s)}; !res) {
        return std::unexpected{res.error()};
    }

    global_logger().verbose("Guest agent is available in machine {}", machine->get_id());

    global_logger().verbose("Waiting for machine {} network.", machine->get_id());
    if (!wait_until_gitea_instance_available(*machine, main_config.forge.uri, 120s)) {
        return std::unexpected{GenericError{std::format(
            "Timed out while waiting for networking to become available in machine {}.", machine->get_id())}};
    }

    global_logger().verbose("Networking is available in machine {}.", machine->get_id());

    return machine;
}

std::expected<void, GenericError> execute_task_in_machine(const ::runner::v1::Task& task, const gitea::Runner& runner,
                                                          const config::MachineTemplateConfig& config,
                                                          Machine& machine) noexcept {
    using namespace std::literals;
    const auto id{task.id()};

    global_logger().verbose("Preparing environment for machine {}.", machine.get_id());

    return Injectables::generate(task, runner)
        .and_then([&](auto res) { return inject_runner_files(machine, std::move(res)); })
        .and_then([&] {
            global_logger().verbose("Executing task #{} in machine {}.", task.id(), machine.get_id());
            return machine
                .shell_exec({config.runner_exe_path, "run-task", "--config", "/tmp/runner_config.yml", "--task",
                             "/tmp/runner_task"})
                .and_then([&](auto res) -> std::expected<void, GenericError> {
                    LOG_SELECT(verbose, error, res.exit_code == 0,
                               "Task #{} execution exited with code {} and output: {}", task.id(), res.exit_code,
                               res.output);
                    return {};
                });
        });
}

struct BackendState {
    BackendState(std::shared_ptr<const config::BackendConfig> backend_config) {}

    static std::shared_ptr<BackendState> create(std::shared_ptr<const config::BackendConfig> backend_config) {
        return std::make_shared<BackendState>(backend_config);
    }
};

struct TemplateState {
    utility::ShutdownSignal stop;
    std::shared_ptr<const config::MainConfig> main_config;
    std::shared_ptr<const config::BackendConfig> backend_config;
    std::shared_ptr<const config::MachineTemplateConfig> template_config;
    std::shared_ptr<gitea::AdminServiceClient> admin_service;
    std::shared_ptr<BackendState> backend_state;
    MachinePool machine_pool;

    TemplateState(std::shared_ptr<const config::MainConfig> main_config,
                  std::shared_ptr<const config::BackendConfig> backend_config,
                  std::shared_ptr<const config::MachineTemplateConfig> template_config,
                  std::shared_ptr<BackendState> backend_state, utility::ShutdownSignal stop)
            : stop{std::move(stop)}, main_config{main_config}, backend_config{backend_config},
              template_config{template_config}, admin_service{std::make_shared<gitea::AdminServiceClient>(
                                                    main_config->forge.uri, main_config->forge.token.resolved_token)},
              machine_pool{create_pool()}, backend_state{std::move(backend_state)} {
        machine_pool.start();
    }

    TemplateState(const TemplateState&) = delete;
    TemplateState(TemplateState&&) = default;
    TemplateState& operator=(const TemplateState&) = delete;
    TemplateState& operator=(TemplateState&&) = default;

    ~TemplateState() { machine_pool.stop(); }

    std::expected<::runner::v1::Task, GenericError> fetch_task(const gitea::Runner& runner) noexcept {
        using namespace std::literals;
        std::optional<::runner::v1::Task> task;
        while (!stop.is_signalled() && !task.has_value()) {
            auto res{try_fetch_task(runner)};
            if (!res) {
                return std::unexpected{res.error()};
            }
            if (!res->has_value()) {
                if (!stop.is_signalled()) {
                    std::this_thread::sleep_for(1s);
                }
                continue;
            }
            task = *std::move(res);
        }
        if (stop.is_signalled()) {
            return std::unexpected{GenericError{"Fetch task loop is shutting down"}};
        }
        return *std::move(task);
    }

    std::expected<gitea::Runner, GenericError> create_runner(const Machine& machine) noexcept {
        return gitea::Runner::connect(
            {
                .forge_uri = main_config->forge.uri,
                .name = std::format("{}-{}", main_config->name, machine.get_id()),
                .labels = template_config->labels,
                .version = std::string{runner_version},
            },
            admin_service);
    }

    void runner_loop() noexcept {
        while (!stop.is_signalled()) {
            if (auto res{runner_loop_iteration()}; !res) {
                global_logger().error("{}", res.error().what());
            }
        }
    }

    std::expected<void, GenericError> runner_loop_iteration() noexcept {
        using namespace std::literals;

        // FIXME: configurable timeout
        auto machine_res{machine_pool.acquire(3h)};
        if (!machine_res) {
            global_logger().error("Failed to acquire machine from pool: {}", machine_res.error().what());
            if (!stop.is_signalled()) {
                std::this_thread::sleep_for(5s);
            }
            return {};
        }

        auto machine{*std::move(machine_res)};
        if (!machine) {
            // Timed out
            if (!stop.is_signalled()) {
                global_logger().error("Timed out while acquiring machine from pool: {}", machine_res.error().what());
                std::this_thread::sleep_for(1s);
            }
            return {};
        }

        utility::Deferred machine_releaser{[&] { machine_pool.release(machine); }};

        if (stop.is_signalled()) {
            return std::unexpected{GenericError{"Runner loop shutting down"}};
        }

        auto runner_res{create_runner(*machine)};
        if (!runner_res) {
            return std::unexpected{runner_res.error()};
        }
        auto runner{*std::move(runner_res)};

        if (stop.is_signalled()) {
            return std::unexpected{GenericError{"Runner loop shutting down"}};
        }

        auto task_res{fetch_task(runner)};
        if (!task_res) {
            return std::unexpected{task_res.error()};
        }
        auto task{*std::move(task_res)};

        global_logger().verbose("Runner {} fetched task with ID {}.", runner.id(), task.id());

        if (stop.is_signalled()) {
            runner.set_task_failed(task);
            return std::unexpected{GenericError{"Runner loop shutting down"}};
        }

        machine_pool.activate(machine);
        utility::Deferred machine_deactivator{[&] { machine_pool.deactivate(machine); }};

        auto exec_res{execute_task_in_machine(task, runner, *template_config, *machine)
                          .or_else([&](auto err) -> std::expected<void, GenericError> {
                              runner.set_task_failed(task);
                              return {};
                          })};
        if (!exec_res) {
            return std::unexpected{exec_res.error()};
        }

        return {};
    }

    std::expected<std::optional<::runner::v1::Task>, GenericError> static try_fetch_task(
        const gitea::Runner& runner) noexcept {
        return runner.fetch_task().transform(
            [](auto res) { return res.has_task() ? std::make_optional(res.task()) : std::nullopt; });
    }

    MachinePool create_pool() {
        MachinePool machine_pool{
            template_config->idle_target, template_config->max_concurrency,
            [this] noexcept { return spawn_machine(*main_config, *backend_config, *template_config); }, stop};
        machine_pool.set_stats_callback([this](auto stats) noexcept {
            global_logger().verbose("{} stats: provisioned: {}; warming: {}; idle: {}; acquiring: {}; "
                                    "acquired: {}; active: {}",
                                    backend_config->name, stats.provisioned, stats.warming, stats.idle, stats.acquiring,
                                    stats.acquired, stats.active);
        });
        return machine_pool;
    }

    static std::shared_ptr<TemplateState> create(std::shared_ptr<const config::MainConfig> main_config,
                                                 std::shared_ptr<const config::BackendConfig> backend_config,
                                                 std::shared_ptr<const config::MachineTemplateConfig> template_config,
                                                 std::shared_ptr<BackendState> backend_state,
                                                 utility::ShutdownSignal stop) {
        return std::make_shared<TemplateState>(std::move(main_config), std::move(backend_config),
                                               std::move(template_config), std::move(backend_state), std::move(stop));
    }
};

size_t count_max_concurrency(const config::MainConfig& main_config) noexcept {
    size_t count{};
    for (auto& backend_config : main_config.backends) {
        for (auto& template_config : backend_config.templates) {
            ++count;
        }
    }
    return count;
}

std::expected<void, GenericError> cmd_daemon(config::MainConfig main_config) noexcept {
    using namespace std::chrono_literals;

    auto stop{utility::ShutdownSignal::install()};
    std::vector<std::shared_ptr<BackendState>> backend_states;
    std::vector<std::shared_ptr<TemplateState>> template_states;
    std::vector<std::jthread> threads;

    backend_states.reserve(main_config.backends.size());
    template_states.reserve(count_max_concurrency(main_config));

    auto shared_main_config{std::make_shared<config::MainConfig>(std::move(main_config))};
    for (auto& backend_config : shared_main_config->backends) {
        auto shared_backend_config{std::make_shared<config::BackendConfig>(std::move(backend_config))};
        auto backend_state{backend_states.emplace_back(BackendState::create(shared_backend_config))};
        for (auto& template_config : shared_backend_config->templates) {
            const auto max_concurrency{template_config.max_concurrency};
            auto shared_template_config{std::make_shared<config::MachineTemplateConfig>(std::move(template_config))};
            auto template_state{template_states.emplace_back(TemplateState::create(
                shared_main_config, shared_backend_config, shared_template_config, backend_state, stop))};
            for (size_t i{}; i < max_concurrency; ++i) {
                threads.emplace_back([template_state] { template_state->runner_loop(); });
            }
        }
    }

    template_states.clear();
    backend_states.clear();

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return {};
}

} // namespace ls_gitea_runner
