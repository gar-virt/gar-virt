find_package(gRPC CONFIG REQUIRED)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)

find_package(CURL REQUIRED)

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

add_library(gitea)
target_sources(gitea PRIVATE
    src/runner_service_client.cpp
)
target_include_directories(gitea PUBLIC include)
target_link_libraries(gitea
    PUBLIC
        proto_objects
    PRIVATE
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
        CURL::libcurl
        utility
)
