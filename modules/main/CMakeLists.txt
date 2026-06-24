find_package(Boost REQUIRED COMPONENTS json program_options CONFIG)
find_package(yaml-cpp REQUIRED)

add_executable(ga_runner)
target_sources(ga_runner PRIVATE
    src/commands/daemon.cpp
    src/config.cpp
    src/main.cpp
)
target_link_libraries(ga_runner PRIVATE
    Boost::json
    Boost::program_options
    yaml-cpp::yaml-cpp
    gitea
    utility
    virt
)

install(TARGETS ga_runner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
