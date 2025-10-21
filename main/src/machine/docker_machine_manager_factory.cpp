#include "docker_machine_manager_factory.hpp"
#include "docker_machine_manager.hpp"

namespace ls_gitea_runner {

docker_machine_manager_factory::~docker_machine_manager_factory() {}

std::unique_ptr<machine_manager> docker_machine_manager_factory::create() {
    return std::make_unique<docker_machine_manager>();
}

} // namespace ls_gitea_runner
