#include "docker_machine_manager.hpp"
#include "../machine/docker_engine_client.hpp"
#include "docker_container_id.hpp"
#include "docker_machine.hpp"
#include "docker_machine_options.hpp"

#include <utility/uuid.hpp>

namespace ls_gitea_runner {

class docker_machine_manager::impl final {
public:
    std::expected<std::unique_ptr<machine>, generic_error> spawn(const std::string& options) {
        const auto parsed_options{docker_machine_options::load(options)};
        if (!parsed_options) {
            return std::unexpected{parsed_options.error()};
        }

        const auto name{std::format("ga_runner-{}", utility::uuid())};

        docker_engine_client docker;
        docker_container_id id;
        std::unique_ptr<docker_machine> machine;

        return docker.container_create(name, parsed_options->image, {"/usr/bin/sh", "-c", "sleep 10800"})
            .and_then([&](auto res) {
                id = std::move(res);
                machine = std::make_unique<docker_machine>(id);
                return docker.container_start(id);
            })
            .transform([&] { return std::move(machine); });
    }
};

docker_machine_manager::docker_machine_manager() : m_impl{std::make_unique<impl>()} {}

docker_machine_manager::~docker_machine_manager() {}

std::expected<std::unique_ptr<machine>, generic_error> docker_machine_manager::spawn(const std::string& options) {
    return m_impl->spawn(options);
}

} // namespace ls_gitea_runner
