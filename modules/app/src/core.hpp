#pragma once

#include "config.hpp"

#include <gitea/admin_service_client.hpp>
#include <gitea/runner.hpp>
#include <runner/v1/messages.pb.h>
#include <utility/shutdown_signal.hpp>
#include <virt/machine_pool.hpp>

#include <expected>
#include <optional>

namespace ls_gitea_runner {

struct TemplateState {
    utility::ShutdownSignal stop;
    std::shared_ptr<const config::MainConfig> main_config;
    std::shared_ptr<const config::BackendConfig> backend_config;
    std::shared_ptr<const config::MachineTemplateConfig> template_config;
    std::shared_ptr<gitea::AdminServiceClient> admin_service;
    MachinePool machine_pool;

    TemplateState(std::shared_ptr<const config::MainConfig> main_config,
                  std::shared_ptr<const config::BackendConfig> backend_config,
                  std::shared_ptr<const config::MachineTemplateConfig> template_config, utility::ShutdownSignal stop);

    TemplateState(const TemplateState&) = delete;
    TemplateState(TemplateState&&) = default;
    TemplateState& operator=(const TemplateState&) = delete;
    TemplateState& operator=(TemplateState&&) = default;

    ~TemplateState();

    std::expected<::runner::v1::Task, GenericError> fetch_task(const gitea::Runner& runner) const;

    std::expected<gitea::Runner, GenericError> create_runner(const Machine& machine);

    void runner_loop();

    std::expected<void, GenericError> runner_loop_iteration();

    std::expected<std::optional<::runner::v1::Task>, GenericError> static try_fetch_task(const gitea::Runner& runner);

    MachinePool create_pool();

    static std::shared_ptr<TemplateState> create(std::shared_ptr<const config::MainConfig> main_config,
                                                 std::shared_ptr<const config::BackendConfig> backend_config,
                                                 std::shared_ptr<const config::MachineTemplateConfig> template_config,
                                                 utility::ShutdownSignal stop);
};

size_t count_max_concurrency(const config::MainConfig& main_config) noexcept;

} // namespace ls_gitea_runner
