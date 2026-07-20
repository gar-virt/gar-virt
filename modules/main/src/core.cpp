#include "core.hpp"

#include "config.hpp"
#include "version.hpp"

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
#include <string>

namespace ls_gitea_runner {

struct Injectables {
    std::string runner_state_json;
    std::string runner_config_yaml;
    std::vector<std::byte> encoded_task;

    static std::expected<Injectables, GenericError> generate(const Machine& machine, const ::runner::v1::Task& task,
                                                             const gitea::Runner& runner);
};

namespace {

template <typename... Args>
void log_select(utility::LogLevel true_level, utility::LogLevel false_level, bool cond,
                std::format_string<Args...> format, Args&&... args) {
    global_logger().log(cond ? true_level : false_level, std::move(format), std::forward<Args>(args)...);
}

std::expected<void, GenericError> inject_runner_files(Machine& machine, Injectables injectables) {
    return machine.write_file(machine.make_temp_path("runner_config.yml"), injectables.runner_config_yaml)
        .and_then([&] { return machine.write_file(machine.make_temp_path(".runner"), injectables.runner_state_json); })
        .and_then([&] { return machine.write_file(machine.make_temp_path("runner_task"), injectables.encoded_task); });
}

std::expected<std::vector<std::string>, GenericError> make_ping_command(const std::string& target_os,
                                                                        const std::string& host) {
    std::vector<std::string> cmd = {"ping"};
    if (utility::string_compare_ci(target_os, "linux") == 0) {
        // Count
        cmd.emplace_back("-c");
        cmd.emplace_back("1");
        // Timeout in seconds
        cmd.emplace_back("-W");
        cmd.emplace_back("2");
    } else if (utility::string_compare_ci(target_os, "macos") == 0) {
        // Count
        cmd.emplace_back("-c");
        cmd.emplace_back("1");
        // Timeout in milliseconds
        cmd.emplace_back("-W");
        cmd.emplace_back("2000");
    } else if (utility::string_compare_ci(target_os, "windows") == 0) {
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
                                                                      std::chrono::seconds timeout,
                                                                      utility::ShutdownSignal stop) {
    using namespace std::literals;
    const auto parsed_instance_url{boost::urls::parse_uri(instance_url)};
    const auto& host{parsed_instance_url->host()};

    const auto cmd{make_ping_command(machine.info().os, host)};
    if (!cmd) {
        return std::unexpected{cmd.error()};
    }

    SpawnResult spawn_res;
    auto start_time{std::chrono::steady_clock::now()};
    while (start_time - std::chrono::steady_clock::now() < timeout) {
        if (stop.is_signalled()) {
            return std::unexpected{GenericError{"Shutting down"}};
        }
        auto res{machine.shell_exec(*cmd, timeout < 10s ? timeout : 10s)};
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
              const config::MachineTemplateConfig& template_config, utility::ShutdownSignal stop) {
    using namespace std::literals;

    const auto& backend_type{backend_config.type};

    auto machine_manager_factory_res{MachineManagerFactorySelector::get_factory(backend_type)};
    if (!machine_manager_factory_res) {
        return std::unexpected{machine_manager_factory_res.error()};
    }

    auto machine_manager_factory{std::move(*machine_manager_factory_res)};
    auto machine_manager{machine_manager_factory->create()};
    const auto arch_name{Arch::to_name(template_config.arch)};

    global_logger().debug("Spawning new {} machine: os = {}; arch = {}", backend_type, template_config.os, arch_name);

    auto machine_res{machine_manager->spawn(
        Machine::Info{
            .os = template_config.os,
            .arch = template_config.arch,
            .temp_dir = template_config.temp_dir,
        },
        backend_config.raw_details, template_config.raw_details, main_config.base_dir)};

    if (!machine_res) {
        return std::unexpected{machine_res.error()};
    }

    auto machine{*std::move(machine_res)};

    global_logger().debug("Spawned new {} machine: os = {}; arch = {}; id = {}", backend_type, template_config.os,
                          arch_name, machine->get_id());

    global_logger().debug("Waiting for machine {} guest agent.", machine->get_id());
    // TODO: configurable timeout
    if (auto res{machine->wait_for_guest_agent(120s, stop)}; !res) {
        return std::unexpected{res.error()};
    }

    global_logger().debug("Guest agent is available in machine {}", machine->get_id());

    global_logger().debug("Waiting for machine {} network.", machine->get_id());
    if (auto res{wait_until_gitea_instance_available(*machine, main_config.forge.uri, 120s, stop)}; !res) {
        return std::unexpected{GenericError{
            std::format("Error while waiting for machine {} network: {}", machine->get_id(), res.error().what())}};
    }

    global_logger().debug("Networking is available in machine {}.", machine->get_id());

    return machine;
}

std::expected<void, GenericError> execute_task_in_machine(const ::runner::v1::Task& task, const gitea::Runner& runner,
                                                          const config::MachineTemplateConfig& config,
                                                          Machine& machine) {
    using namespace std::literals;
    const auto id{task.id()};

    global_logger().debug("Preparing environment for machine {}.", machine.get_id());

    return Injectables::generate(machine, task, runner)
        .and_then([&](auto res) { return inject_runner_files(machine, std::move(res)); })
        .and_then([&] {
            global_logger().debug("Executing task #{} in machine {}.", id, machine.get_id());
            return machine
                .shell_exec({config.runner_exe_path, "run-task", "--config",
                             machine.make_temp_path("runner_config.yml"), "--task",
                             machine.make_temp_path("runner_task")},
                            3h) // TODO: configurable timeout
                .and_then([&](auto res) -> std::expected<void, GenericError> {
                    log_select(utility::LogLevel::debug, utility::LogLevel::error, res.exit_code == 0,
                               "Task #{} execution exited with code {} and output: {}", id, res.exit_code, res.output);
                    return {};
                });
        });
}

} // namespace

std::expected<Injectables, GenericError> Injectables::generate(const Machine& machine, const ::runner::v1::Task& task,
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
        // TODO: Allow user-provided Gitea Runner config
        .runner_config_yaml = std::format(R"(runner:
  file: {}
)",
                                          machine.make_temp_path(".runner")),
        .encoded_task = *encode_payload,
    };
}

TemplateState::TemplateState(std::shared_ptr<const config::MainConfig> main_config,
                             std::shared_ptr<const config::BackendConfig> backend_config,
                             std::shared_ptr<const config::MachineTemplateConfig> template_config,
                             utility::ShutdownSignal stop)
        : stop{std::move(stop)}, main_config{main_config}, backend_config{backend_config},
          template_config{template_config}, admin_service{std::make_shared<gitea::AdminServiceClient>(
                                                main_config->forge.uri, main_config->forge.token.resolved_token)},
          machine_pool{create_pool()} {
    machine_pool.start();
}

TemplateState::~TemplateState() { machine_pool.stop(); }

std::expected<::runner::v1::Task, GenericError> TemplateState::fetch_task(const gitea::Runner& runner) const {
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

std::expected<gitea::Runner, GenericError> TemplateState::create_runner(const Machine& machine) {
    return gitea::Runner::connect(
        {
            .forge_uri = main_config->forge.uri,
            .name = std::format("{}-{}", main_config->name, machine.get_id()),
            .labels = template_config->labels,
            .version = std::string{runner_version},
        },
        admin_service);
}

void TemplateState::runner_loop() {
    while (!stop.is_signalled()) {
        if (auto res{runner_loop_iteration()}; !res) {
            global_logger().error("{}", res.error().what());
        }
    }
}

std::expected<void, GenericError> TemplateState::runner_loop_iteration() {
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
            global_logger().error("Timed out while acquiring machine {} from pool", machine->get_id());
            std::this_thread::sleep_for(1s);
        }
        return {};
    }

    const utility::Deferred machine_releaser{[&] { machine_pool.release(machine); }};

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

    global_logger().debug("Runner {} fetched task with ID {}.", runner.id(), task.id());

    if (stop.is_signalled()) {
        runner.set_task_failed(task);
        return std::unexpected{GenericError{"Runner loop shutting down"}};
    }

    machine_pool.activate(machine);
    const utility::Deferred machine_deactivator{[&] { machine_pool.deactivate(machine); }};

    auto exec_res{execute_task_in_machine(task, runner, *template_config, *machine)
                      .or_else([&](auto) -> std::expected<void, GenericError> {
                          runner.set_task_failed(task);
                          return {};
                      })};
    if (!exec_res) {
        return std::unexpected{exec_res.error()};
    }

    return {};
}

std::expected<std::optional<::runner::v1::Task>, GenericError>
TemplateState::try_fetch_task(const gitea::Runner& runner) {
    return runner.fetch_task().transform(
        [](auto res) { return res.has_task() ? std::make_optional(res.task()) : std::nullopt; });
}

MachinePool TemplateState::create_pool() {
    MachinePool machine_pool{template_config->idle_target, template_config->max_concurrency,
                             [this] { return spawn_machine(*main_config, *backend_config, *template_config, stop); },
                             stop};
    machine_pool.set_stats_callback([this](auto stats) noexcept {
        global_logger().debug("{} stats: provisioned: {}; warming: {}; idle: {}; acquiring: {}; "
                              "acquired: {}; active: {}",
                              backend_config->name, stats.provisioned, stats.warming, stats.idle, stats.acquiring,
                              stats.acquired, stats.active);
    });
    return machine_pool;
}

std::shared_ptr<TemplateState> TemplateState::create(
    std::shared_ptr<const config::MainConfig> main_config, std::shared_ptr<const config::BackendConfig> backend_config,
    std::shared_ptr<const config::MachineTemplateConfig> template_config, utility::ShutdownSignal stop) {
    return std::make_shared<TemplateState>(std::move(main_config), std::move(backend_config),
                                           std::move(template_config), std::move(stop));
}

size_t count_max_concurrency(const config::MainConfig& main_config) noexcept {
    size_t count{};
    for (const auto& backend_config : main_config.backends) {
        count += backend_config.templates.size();
    }
    return count;
}

} // namespace ls_gitea_runner
