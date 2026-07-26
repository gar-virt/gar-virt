#pragma once

#include "machine.hpp"

#include <utility/error.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace gv {

class MachineManager {
public:
    virtual ~MachineManager() = default;
    virtual std::expected<std::unique_ptr<Machine>, Error> spawn(const Machine::Info& info,
                                                                        const std::string& serialized_pool_details,
                                                                        const std::string& serialized_template_details,
                                                                        const std::filesystem::path& config_dir) = 0;
};

} // namespace gv
