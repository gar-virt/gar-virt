find_package(Boost REQUIRED COMPONENTS json program_options url CONFIG)
find_package(yaml-cpp REQUIRED CONFIG)

add_executable(app)
set_target_properties(app PROPERTIES
    OUTPUT_NAME gar-virt
)
target_sources(app
    PRIVATE
        FILE_SET CXX_MODULES
        FILES
            src/app_main.cpp
            src/app_module.cpp
            src/commands.cpp
            src/config.cpp
            src/config_impl.cpp
            src/core.cpp
            src/version.cpp
    PRIVATE
        src/main.cpp
)
target_link_libraries(app PRIVATE
    Boost::json
    Boost::program_options
    Boost::url
    yaml-cpp::yaml-cpp
    checks
    gitea
    utility
    virt
)

install(TARGETS app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
