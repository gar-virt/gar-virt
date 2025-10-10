cmake_minimum_required(VERSION 4.0)
project(gitea_runner)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

#include(cmake/CPM.cmake)
#CPMAddPackage("gh:grpc/grpc#v1.75.1")

#include(FetchContent)
#FetchContent_Declare(
#  gRPC
#  GIT_REPOSITORY https://github.com/grpc/grpc
#  GIT_TAG        v1.75.1
#)
#set(FETCHCONTENT_QUIET OFF)
#FetchContent_MakeAvailable(gRPC)

add_subdirectory(utility)
add_subdirectory(main)
