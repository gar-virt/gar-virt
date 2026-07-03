#pragma once

#include <utility/error.hpp>
#include "machine_manager_factory.hpp"

#include <memory>

namespace ls_gitea_runner {

class MachineManagerFactorySelector final {
public:
    static std::expected<std::unique_ptr<MachineManagerFactory>, GenericError> get_factory(const std::string& name);
};

} // namespace ls_gitea_runner
