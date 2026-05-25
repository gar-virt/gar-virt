#pragma once

#include "../error.hpp"
#include "machine.hpp"
#include "machine_manager.hpp"

#include <expected>
#include <memory>

namespace ls_gitea_runner {

class DockerMachineManager final : public MachineManager {
public:
    DockerMachineManager();
    ~DockerMachineManager();

    std::expected<std::unique_ptr<Machine>, GenericError> spawn(Machine::Info info,
                                                                const std::string& details) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ls_gitea_runner
