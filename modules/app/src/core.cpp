module;

#include <runner/v1/messages.pb.h>

module app:core;

import :config;

import gitea;
import utility.misc;
import virt;

import std;

namespace ls_gitea_runner {

struct TemplateState {
    utility::ShutdownSignal stop;
    std::shared_ptr<const config::MainConfig> main_config;
    std::shared_ptr<const config::BackendConfig> backend_config;
    std::shared_ptr<const config::MachineTemplateConfig> template_config;
    std::shared_ptr<gitea::AdminServiceClient> admin_service;
    MachinePool machine_pool;

    TemplateState(std::shared_ptr<const config::MainConfig> main_config_,
                  std::shared_ptr<const config::BackendConfig> backend_config,
                  std::shared_ptr<const config::MachineTemplateConfig> template_config, utility::ShutdownSignal stop);

    ~TemplateState();

    std::expected<::runner::v1::Task, GenericError> fetch_task(const gitea::Runner& runner) const;

    std::expected<gitea::Runner, GenericError> create_runner(const Machine& machine) const;

    void runner_loop();

    std::expected<void, GenericError> runner_loop_iteration();

    static std::expected<std::optional<::runner::v1::Task>, GenericError> try_fetch_task(const gitea::Runner& runner);

    MachinePool create_pool() const;

    static std::shared_ptr<TemplateState> create(std::shared_ptr<const config::MainConfig> main_config,
                                                 std::shared_ptr<const config::BackendConfig> backend_config,
                                                 std::shared_ptr<const config::MachineTemplateConfig> template_config,
                                                 utility::ShutdownSignal stop);
};

size_t count_max_concurrency(const config::MainConfig& main_config) noexcept;

} // namespace ls_gitea_runner
