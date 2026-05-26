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
  URL https://github.com/boostorg/boost/releases/download/boost-1.89.0/boost-1.89.0-cmake.7z
  URL_HASH SHA256=252149f583054515b10688855b24ca1c16072b5118e603b8fe017894d319c262
  EXCLUDE_FROM_ALL
  SYSTEM
  OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(Boost)

add_subdirectory(modules)
