#include "../config.hpp"
#include "../machine/machine_manager_factory_selector.hpp"
#include "../state.hpp"

#include <iostream>
#include <print>

namespace ls_gitea_runner {

int cmd_test(config::runner_config config, runtime_state state) noexcept {
    using namespace std::literals;
    auto machine_manager_factory_res{machine_manager_factory_selector::get_factory("docker")};
    if (!machine_manager_factory_res) {
        std::println(std::cerr, "Error: {}", machine_manager_factory_res.error().what());
        return 1;
    }
    auto machine_manager_factory{std::move(*machine_manager_factory_res)};
    auto machine_manager{machine_manager_factory->create()};
    auto machine_res{machine_manager->spawn(R"({"image": "ubuntu:24.04"})")};
    if (!machine_res) {
        std::println(std::cerr, "Error: {}", machine_res.error().what());
        return 1;
    }
    auto machine{std::move(*machine_res)};
    if (!machine->wait_until_ready(30s)) {
        std::println(std::cerr, "Error: Timed out while waiting for machine to become ready");
        return 1;
    }
    auto exec_res{machine->shell_exec({"/usr/bin/sh", "-c", "echo yahoo"})};
    if (!exec_res) {
        std::println(std::cerr, "Error: {}", exec_res.error().what());
        return 1;
    }
    std::println("Output: {}", exec_res->output);
    return 0;
}

} // namespace ls_gitea_runner
