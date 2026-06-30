cmake_minimum_required(VERSION 3.28...4.3)
project(gitea_runner)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

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
FetchContent_Declare(
  cppcodec
  GIT_REPOSITORY https://github.com/tplgy/cppcodec.git
  GIT_TAG 8019b8b580f8573c33c50372baec7039dfe5a8ce
  GIT_SUBMODULES ""
  SYSTEM
  OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(Boost cppcodec)

add_subdirectory(modules)
