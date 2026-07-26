#pragma once

#include "machine_manager_factory.hpp"
#include <utility/error.hpp>

#include <memory>

namespace gv {

class MachineManagerFactorySelector final {
public:
    static std::expected<std::unique_ptr<MachineManagerFactory>, Error> get_factory(const std::string& name);
};

} // namespace gv
