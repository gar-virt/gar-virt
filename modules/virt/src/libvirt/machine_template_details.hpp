#pragma once

#include <utility/result.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace gv::virt {

struct LibvirtMachineTemplateDetails {
    std::filesystem::path domain_template_path;
    std::filesystem::path volume_template_path;
    std::string storage_pool_name;

    static Result<LibvirtMachineTemplateDetails> load(const std::string& details,
                                                      const std::filesystem::path& config_dir);
};

} // namespace gv::virt
