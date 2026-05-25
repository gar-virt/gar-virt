#include "machine_manager.hpp"
#include "container_id.hpp"
#include "engine_client.hpp"
#include "machine.hpp"
#include "machine_options.hpp"

#include <utility/uuid.hpp>

namespace ls_gitea_runner {

class DockerMachineManager::Impl final {
public:
    std::expected<std::unique_ptr<Machine>, GenericError> spawn(Machine::Info info, const std::string& details) {
        const auto parsed_options{DockerMachineOptions::load(details)};
        if (!parsed_options) {
            return std::unexpected{parsed_options.error()};
        }

        const auto name{std::format("ga_runner-{}", utility::uuid())};

        DockerEngineClient docker;
        DockerContainerId id;
        std::unique_ptr<DockerMachine> machine;

        return docker.container_create(name, parsed_options->image, {"/usr/bin/sh", "-c", "sleep 10800"})
            .and_then([&](auto res) {
                id = std::move(res);
                machine = std::make_unique<DockerMachine>(id, std::move(info));
                return docker.container_start(id);
            })
            .transform([&] { return std::move(machine); });
    }
};

DockerMachineManager::DockerMachineManager() : m_impl{std::make_unique<Impl>()} {}

DockerMachineManager::~DockerMachineManager() {}

std::expected<std::unique_ptr<Machine>, GenericError> DockerMachineManager::spawn(Machine::Info info,
                                                                                  const std::string& details) {
    return m_impl->spawn(std::move(info), details);
}

} // namespace ls_gitea_runner
