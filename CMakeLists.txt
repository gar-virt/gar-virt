cmake_minimum_required(VERSION 3.28...4.3)

project(gar_virt
    VERSION 0.1.0
    DESCRIPTION "A simple Gitea Actions runner controller/orchestrator for self-hosted runner autoscaling with local and remote libvirt/QEMU support."
    HOMEPAGE_URL "https://github.com/gar-virt/gar-virt"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

include(GNUInstallDirs)
include(cmake/sanitizers.cmake)
include(cmake/dependencies.cmake)

add_subdirectory(modules)
