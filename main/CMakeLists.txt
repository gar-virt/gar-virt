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
target_sources(ga_runner PRIVATE src/main.cpp)
target_link_libraries(ga_runner PRIVATE protobuf::libprotobuf gRPC::grpc gRPC::grpc++ Boost::json Boost::program_options CURL::libcurl yaml-cpp::yaml-cpp ${MUJS_LIBRARY} proto_objects utility)
