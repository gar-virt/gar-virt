find_package(Boost REQUIRED COMPONENTS json program_options url CONFIG)
find_package(yaml-cpp REQUIRED CONFIG)

add_executable(gar_virt)
set_target_properties(gar_virt PROPERTIES
    OUTPUT_NAME gar-virt
)
target_sources(gar_virt PRIVATE
    src/commands.cpp
    src/config.cpp
    src/core.cpp
    src/main.cpp
)
target_link_libraries(gar_virt PRIVATE
    Boost::json
    Boost::program_options
    Boost::url
    yaml-cpp::yaml-cpp
    checks
    gitea
    utility
    virt
)

install(TARGETS gar_virt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
