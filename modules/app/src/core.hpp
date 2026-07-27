#pragma once

#include "config.hpp"

#include <gitea/admin_service_client.hpp>
#include <gitea/runner.hpp>
#include <utility/shutdown_signal.hpp>
#include <virt/api.hpp>

#include <expected>

namespace gv {

struct TemplateState {
    utility::ShutdownSignal stop;
    std::shared_ptr<const config::MainConfig> main_config;
    std::shared_ptr<const config::BackendConfig> backend_config;
    std::shared_ptr<const config::MachineTemplateConfig> template_config;
    std::shared_ptr<gitea::AdminServiceClient> admin_service;
    virt::MachinePool machine_pool;

    TemplateState(std::shared_ptr<const config::MainConfig> main_config,
                  std::shared_ptr<const config::BackendConfig> backend_config,
                  std::shared_ptr<const config::MachineTemplateConfig> template_config, utility::ShutdownSignal stop);

    TemplateState(const TemplateState&) = delete;
    TemplateState(TemplateState&&) = default;
    TemplateState& operator=(const TemplateState&) = delete;
    TemplateState& operator=(TemplateState&&) = default;

    ~TemplateState();

    Result<gitea::TaskParcel> fetch_task(const gitea::Runner& runner) const;

    Result<gitea::Runner> create_runner(const virt::Machine& machine);

    void runner_loop();

    Result<void> runner_loop_iteration();

    virt::MachinePool create_pool();

    static std::shared_ptr<TemplateState> create(std::shared_ptr<const config::MainConfig> main_config,
                                                 std::shared_ptr<const config::BackendConfig> backend_config,
                                                 std::shared_ptr<const config::MachineTemplateConfig> template_config,
                                                 utility::ShutdownSignal stop);
};

size_t count_max_concurrency(const config::MainConfig& main_config) noexcept;

} // namespace gv
