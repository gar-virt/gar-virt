find_package(gRPC CONFIG REQUIRED)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)

find_package(Boost REQUIRED COMPONENTS json program_options CONFIG)
find_package(CURL REQUIRED)
find_package(yaml-cpp REQUIRED)

#find_package(mujs REQUIRED)
find_library(MUJS_LIBRARY mujs REQUIRED)

add_library(proto_objects OBJECT)
target_sources(proto_objects PRIVATE
    proto/ping/v1/messages.proto
    proto/ping/v1/services.proto
    proto/runner/v1/messages.proto
    proto/runner/v1/services.proto
)
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}/proto")

target_include_directories(proto_objects PUBLIC "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>")

protobuf_generate(
    TARGET proto_objects
    IMPORT_DIRS ${PROTO_IMPORT_DIRS}
    PROTOC_OUT_DIR ${PROTO_BINARY_DIR}
)

protobuf_generate(
    TARGET proto_objects
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
    IMPORT_DIRS ${PROTO_IMPORT_DIRS}
    PROTOC_OUT_DIR ${PROTO_BINARY_DIR}
)

add_executable(ga_runner)
target_sources(ga_runner PRIVATE
    src/commands/daemon.cpp
    src/commands/register.cpp
    src/config.cpp
    src/error.cpp
    src/gitea/gitea_log_reporter.cpp
    src/gitea/gitea_runner_service_client.cpp
    src/gitea/gitea_runner_task_poller.cpp
    src/gitea/gitea_runner_task_processor.cpp
    src/gitea/gitea_workflow.cpp
    src/gitea/gitea_workflow_executor.cpp
    src/http.cpp
    src/machine/docker_engine_client.cpp
    src/machine/docker_machine.cpp
    src/machine/docker_machine_manager.cpp
    src/machine/docker_machine_manager_factory.cpp
    src/machine/docker_machine_options.cpp
    src/machine/machine_manager_factory_selector.cpp
    src/main.cpp
    src/protobuf_helper.cpp
    src/scripting.cpp
    src/state.cpp
)
target_link_libraries(ga_runner PRIVATE
    protobuf::libprotobuf
    gRPC::grpc
    gRPC::grpc++
    Boost::json
    Boost::program_options
    CURL::libcurl
    yaml-cpp::yaml-cpp
    ${MUJS_LIBRARY}
    proto_objects
    utility
)

install(TARGETS ga_runner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
