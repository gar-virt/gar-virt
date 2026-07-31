include(FindPackageHandleStandardArgs)

find_path(libvirt_INCLUDE_DIR NAMES libvirt/libvirt.h)
find_path(libvirt_qemu_INCLUDE_DIR NAMES libvirt/libvirt-qemu.h)
find_library(libvirt_LIBRARY NAMES virt)
find_library(libvirt_qemu_LIBRARY NAMES virt-qemu)

set(libvirt_LIBRARIES ${libvirt_LIBRARY} ${libvirt_qemu_LIBRARY})

find_package_handle_standard_args(
    libvirt
    DEFAULT_MSG
    libvirt_INCLUDE_DIR
    libvirt_LIBRARY
    libvirt_qemu_LIBRARY
    libvirt_LIBRARIES)

mark_as_advanced(libvirt_INCLUDE_DIR libvirt_LIBRARY libvirt_qemu_LIBRARY libvirt_LIBRARIES)

if(libvirt_FOUND)
    if (NOT TARGET libvirt::virt_headers)
        add_library(libvirt::virt_headers INTERFACE IMPORTED)
        target_include_directories(libvirt::virt_headers SYSTEM INTERFACE ${libvirt_INCLUDE_DIR})
    endif()
    if (NOT TARGET libvirt::virt)
        add_library(libvirt::virt SHARED IMPORTED)
        target_link_libraries(libvirt::virt INTERFACE libvirt::virt_headers)
        set_target_properties(
            libvirt::virt
            PROPERTIES
                IMPORTED_LOCATION ${libvirt_LIBRARY}
        )
    endif()
    if(NOT TARGET libvirt::qemu_headers)
        add_library(libvirt::qemu_headers INTERFACE IMPORTED)
        target_include_directories(libvirt::qemu_headers SYSTEM INTERFACE ${libvirt_qemu_INCLUDE_DIR})
    endif()
    if(NOT TARGET libvirt::qemu)
        add_library(libvirt::qemu SHARED IMPORTED)
        target_link_libraries(libvirt::qemu INTERFACE libvirt::qemu_headers)
        set_target_properties(
            libvirt::qemu
            PROPERTIES
                IMPORTED_LOCATION ${libvirt_qemu_LIBRARY}
        )
    endif()
endif()
