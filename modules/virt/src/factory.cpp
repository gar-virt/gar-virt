#include <virt/api.hpp>

#include "libvirt/factory.hpp"

#include <utility/result.hpp>

#include <format>
#include <memory>
#include <string>

namespace gv::virt {

Result<std::unique_ptr<Backend>> create_backend(const std::string& name) {
    if (name == "libvirt") {
        return create_libvirt_backend();
    }
    return std::unexpected{Error{std::format("Unknown backend: {}", name)}};
}

} // namespace gv::virt
