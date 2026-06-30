cmake_minimum_required(VERSION 3.28...4.3)
project(gitea_runner)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

include(GNUInstallDirs)
include(cmake/dependencies.cmake)

add_subdirectory(modules)
