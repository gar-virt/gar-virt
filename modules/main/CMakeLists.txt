find_package(Boost REQUIRED COMPONENTS json program_options CONFIG)
find_package(libvirt REQUIRED)

add_executable(ga_runner)
target_sources(ga_runner PRIVATE
    src/commands/daemon.cpp
    src/config.cpp
    src/libvirt/libvirt.cpp
    src/libvirt/machine.cpp
    src/libvirt/machine_manager.cpp
    src/libvirt/machine_manager_factory.cpp
    src/libvirt/machine_options.cpp
    src/machine/machine_manager_factory_selector.cpp
    src/main.cpp
)
target_link_libraries(ga_runner PRIVATE
    Boost::json
    Boost::program_options
    libvirt::virt
    libvirt::qemu
    gitea
    utility
)

install(TARGETS ga_runner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
