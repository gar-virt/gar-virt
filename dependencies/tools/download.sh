#!/usr/bin/env bash
set -e

script_dir=$(dirname "$(realpath "${BASH_SOURCE[@]}")")
download_dir="${script_dir}/downloaded"

mkdir -p "${download_dir}"
cd "${download_dir}"

cmake_version=4.1.2
if [[ ! -f "cmake-${cmake_version}.sh" ]]; then
    echo "Downloading CMake ${cmake_version}"
    curl -sSLo "cmake-${cmake_version}.sh" "https://github.com/Kitware/CMake/releases/download/v${cmake_version}/cmake-${cmake_version}-linux-x86_64.sh"
    chmod +x "cmake-${cmake_version}.sh"
fi

ninja_version=1.13.1
if [[ ! -f "ninja-${ninja_version}.zip" ]]; then
    echo "Downloading Ninja ${ninja_version}"
    curl -sSLo "ninja-${ninja_version}.zip" "https://github.com/ninja-build/ninja/releases/download/v${ninja_version}/ninja-linux.zip"
fi

cd "${script_dir}"
sha256sum -c files.sha256
