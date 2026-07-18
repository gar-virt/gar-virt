find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)
find_package(CURL REQUIRED)

add_library(gitea)
target_sources(gitea PRIVATE
    src/admin_service_client.cpp
    src/runner.cpp
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
        checks
        utility
)
