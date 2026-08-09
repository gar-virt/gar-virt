#!/usr/bin/env bash
set -e
deps/fetch.sh --build-dir build
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
cpack -G TXZ -B build/dist --config build/CPackConfig.cmake -DCPACK_COMPONENTS_ALL='app_archive'
cpack -G DEB -B build/dist --config build/CPackConfig.cmake -DCPACK_COMPONENTS_ALL='app_package'
