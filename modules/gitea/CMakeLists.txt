find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(CURL REQUIRED)

add_library(gitea)
target_sources(gitea PUBLIC FILE_SET CXX_MODULES FILES
    src/admin_service_client_impl.cpp
    src/admin_service_client.cpp
    src/gitea_module.cpp
    src/proto.cpp
    src/runner_credentials.cpp
    src/runner_impl.cpp
    src/runner_service_client.cpp
    src/runner.cpp
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
        checks
        utility
)
