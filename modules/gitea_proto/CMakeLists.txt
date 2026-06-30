find_package(gRPC REQUIRED)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
find_package(Protobuf REQUIRED)
find_package(CURL REQUIRED)

add_library(proto_objects OBJECT)
target_sources(proto_objects PRIVATE
    ping/v1/messages.proto
    ping/v1/services.proto
    runner/v1/messages.proto
    runner/v1/services.proto
)
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}")

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
