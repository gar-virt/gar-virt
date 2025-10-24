#pragma once

#include "../error.hpp"
#include "machine.hpp"
#include "machine_manager.hpp"

#include <expected>
#include <memory>

namespace ls_gitea_runner {

class docker_machine_manager final : public machine_manager {
public:
    docker_machine_manager();
    ~docker_machine_manager();

    std::expected<std::unique_ptr<machine>, generic_error> spawn(const std::string& options) override;

private:
    class impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace ls_gitea_runner
