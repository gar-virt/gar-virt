#pragma once

#include "config.hpp"

#include <gitea/admin_service_client.hpp>
#include <gitea/runner.hpp>
#include <runner/v1/messages.pb.h>
#include <utility/shutdown_signal.hpp>
#include <virt/machine_pool.hpp>

#include <expected>
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

std::expected<void, GenericError> inject_runner_files(Machine& machine, Injectables injectables);

std::expected<std::vector<std::string>, GenericError> make_ping_command(const std::string& target_os,
                                                                        const std::string& host);

std::expected<void, GenericError> wait_until_gitea_instance_available(Machine& machine, const std::string& instance_url,
                                                                      std::chrono::seconds timeout);

std::expected<std::unique_ptr<Machine>, GenericError>
spawn_machine(const config::MainConfig& main_config, const config::BackendConfig& backend_config,
              const config::MachineTemplateConfig& template_config) noexcept;

std::expected<void, GenericError> execute_task_in_machine(const ::runner::v1::Task& task, const gitea::Runner& runner,
                                                          const config::MachineTemplateConfig& config,
                                                          Machine& machine) noexcept;

struct BackendState {
    BackendState(std::shared_ptr<const config::BackendConfig> backend_config);

    static std::shared_ptr<BackendState> create(std::shared_ptr<const config::BackendConfig> backend_config);
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
                  std::shared_ptr<BackendState> backend_state, utility::ShutdownSignal stop);

    TemplateState(const TemplateState&) = delete;
    TemplateState(TemplateState&&) = default;
    TemplateState& operator=(const TemplateState&) = delete;
    TemplateState& operator=(TemplateState&&) = default;

    ~TemplateState();

    std::expected<::runner::v1::Task, GenericError> fetch_task(const gitea::Runner& runner) noexcept;

    std::expected<gitea::Runner, GenericError> create_runner(const Machine& machine) noexcept;

    void runner_loop() noexcept;

    std::expected<void, GenericError> runner_loop_iteration() noexcept;

    std::expected<std::optional<::runner::v1::Task>, GenericError> static try_fetch_task(
        const gitea::Runner& runner) noexcept;

    MachinePool create_pool();

    static std::shared_ptr<TemplateState> create(std::shared_ptr<const config::MainConfig> main_config,
                                                 std::shared_ptr<const config::BackendConfig> backend_config,
                                                 std::shared_ptr<const config::MachineTemplateConfig> template_config,
                                                 std::shared_ptr<BackendState> backend_state,
                                                 utility::ShutdownSignal stop);
};

size_t count_max_concurrency(const config::MainConfig& main_config) noexcept;

} // namespace ls_gitea_runner
