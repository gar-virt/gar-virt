#include <virt/api.hpp>

#include <utility/result.hpp>

#include <memory>

namespace gv::virt {

Result<std::unique_ptr<Backend>> create_libvirt_backend();

} // namespace gv::virt
