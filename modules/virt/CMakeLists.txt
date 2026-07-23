find_package(Boost REQUIRED COMPONENTS json CONFIG)
find_package(libvirt REQUIRED)

add_library(virt)
target_sources(virt
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            src/libvirt/libvirt.cpp
            src/libvirt/libvirt_backend.cpp
            src/api.cpp
            src/virt_module.cpp
)
target_link_libraries(virt PRIVATE
    Boost::json
    libvirt::virt
    libvirt::qemu
    checks
    utility
)
