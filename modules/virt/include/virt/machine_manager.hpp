#pragma once

#include "machine.hpp"

#include <utility/result.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace gv::virt {

class MachineManager {
public:
    virtual ~MachineManager() = default;
    virtual Result<std::unique_ptr<Machine>> spawn(const Machine::Info& info,
                                                   const std::string& serialized_pool_details,
                                                   const std::string& serialized_template_details,
                                                   const std::filesystem::path& config_dir) = 0;
};

} // namespace gv::virt
