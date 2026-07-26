find_package(Boost REQUIRED COMPONENTS json CONFIG)
find_package(libvirt REQUIRED)

add_library(virt)
target_sources(virt PRIVATE
    src/arch.cpp
    src/factory.cpp
    src/libvirt/backend.cpp
    src/libvirt/libvirt.cpp
    src/machine_pool.cpp
    src/machine.cpp
)
target_include_directories(virt PUBLIC include)
target_link_libraries(virt PRIVATE
    Boost::json
    libvirt::virt
    libvirt::qemu
    checks
    utility
)
