cmake_minimum_required(VERSION 4.0...4.3)
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

include(GNUInstallDirs)
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(BOOST_INCLUDE_LIBRARIES)
FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.7z
  URL_HASH SHA256=29c7d4f4ac36ad853b6765d03571ea60d90286775df026b4efd9f3281131972b
  EXCLUDE_FROM_ALL
  SYSTEM
  OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(Boost)

add_subdirectory(modules)
