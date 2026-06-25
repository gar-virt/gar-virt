find_package(Boost REQUIRED COMPONENTS json program_options url CONFIG)
find_package(yaml-cpp REQUIRED)

add_executable(ga_runner)
target_sources(ga_runner PRIVATE
    src/commands/daemon.cpp
    src/config.cpp
    src/log.cpp
    src/main.cpp
)
target_link_libraries(ga_runner PRIVATE
    Boost::json
    Boost::program_options
    Boost::url
    yaml-cpp::yaml-cpp
    gitea
    utility
    virt
)

install(TARGETS ga_runner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
