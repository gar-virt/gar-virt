#pragma once

#include "machine_manager_factory.hpp"
#include <utility/result.hpp>

#include <memory>

namespace gv {

class MachineManagerFactorySelector final {
public:
    static Result<std::unique_ptr<MachineManagerFactory>> get_factory(const std::string& name);
};

} // namespace gv
