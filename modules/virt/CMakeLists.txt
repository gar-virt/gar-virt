find_package(Boost REQUIRED COMPONENTS json CONFIG)
find_package(libvirt REQUIRED)

add_library(virt)
target_sources(virt PRIVATE
    src/libvirt/libvirt.cpp
    src/libvirt/machine.cpp
    src/libvirt/machine_manager.cpp
    src/libvirt/machine_manager_factory.cpp
    src/libvirt/machine_pool_details.cpp
    src/libvirt/machine_template_details.cpp
    src/machine_manager_factory_selector.cpp
    src/machine_pool.cpp
)
target_include_directories(virt PUBLIC include)
target_link_libraries(virt PRIVATE
    Boost::json
    libvirt::virt
    libvirt::qemu
    utility
)
