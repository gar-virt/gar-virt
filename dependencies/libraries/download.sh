#!/usr/bin/env bash
set -e

script_dir=$(dirname "$(realpath "${BASH_SOURCE[@]}")")
download_dir="${script_dir}/downloaded"

mkdir -p "${download_dir}"
cd "${download_dir}"

boost_version=1.89.0
if [[ ! -f "boost-${boost_version}.7z" ]]; then
    echo "Downloading boost ${boost_version}"
    curl -sSLo "boost-${boost_version}.7z" "https://github.com/boostorg/boost/releases/download/boost-${boost_version}/boost-${boost_version}-cmake.7z"
fi

cd "${script_dir}"
sha256sum -c files.sha256
