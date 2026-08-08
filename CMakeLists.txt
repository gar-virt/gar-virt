cmake_minimum_required(VERSION 3.28...4.3)

project(gar_virt
    VERSION 0.1.0
    DESCRIPTION "A simple Gitea Actions runner controller/orchestrator for self-hosted runner autoscaling with local and remote libvirt/QEMU support."
    HOMEPAGE_URL "https://github.com/gar-virt/gar-virt"
    LANGUAGES CXX)

include(GNUInstallDirs)
include(cmake/project_settings.cmake)
include(cmake/static_linking.cmake)
include(cmake/checks.cmake)
include(cmake/sanitizers.cmake)

add_subdirectory(modules)
add_subdirectory(packaging)
