#!/usr/bin/env bash
set -euo pipefail

is_nightly=0
build_deps=1
build_main=1
scm_version=

while (( $# > 0 )); do
    arg_name=$1
    shift
    case "${arg_name}" in
        --nightly)
            is_nightly=1
            ;;
        --only-deps)
            build_main=0
            ;;
        --only-main)
            build_deps=0
            ;;
        *)
            echo "Invalid argument: ${arg_name}"
            exit 1
            ;;
    esac
done

if [[ "${build_deps}" == "1" ]]; then
    deps/fetch.sh --build-dir build
fi

if [[ "${build_main}" == "1" ]]; then
    configure_args=(-DCMAKE_BUILD_TYPE=Release)
    cpack_args=()
    if [[ "${is_nightly}" == "1" ]]; then
        cpack_args+=(-DGARVIRT_NIGHTLY=ON)
    fi
    cmake -G Ninja -B build -S . "${configure_args[@]}"
    cmake --build build
    cpack -G TXZ -B build/dist --config build/CPackConfig.cmake -DCPACK_COMPONENTS_ALL='app_archive' "${cpack_args[@]}"
    cpack -G DEB -B build/dist --config build/CPackConfig.cmake -DCPACK_COMPONENTS_ALL='app_package' "${cpack_args[@]}"
fi
