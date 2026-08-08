find_package(Boost REQUIRED COMPONENTS filesystem json CONFIG)

set(LIBVIRT_HEADER_ONLY TRUE)
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
    Boost::headers
    Boost::filesystem
    Boost::json
    libvirt::virt_headers
    libvirt::qemu_headers
    checks
    utility
)
