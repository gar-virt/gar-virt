#!/usr/bin/env bash
set -e

declare -ra allowed_libraries=(
    ld-linux-x86-64.so.2
    libc.so.6
    libdl.so.2
    libm.so.6
    libpthread.so.0
    librt.so.1
)

max_glibc_major_version=2
max_glibc_minor_version=27
max_glibc_version=${max_glibc_major_version}.${max_glibc_minor_version}

declare -A allowed_libraries_map
for lib in ${allowed_libraries[@]}; do
    allowed_libraries_map[${lib}]=1
done

for input_file in "${@}"; do
    echo "Checking portability: ${input_file}"

    present_libraries=($(readelf --dynamic "${input_file}" | grep NEEDED | cut -d[ -f2 | cut -d] -f1 | sort))

    for lib in "${present_libraries[@]}"; do
        if [[ "${allowed_libraries_map[${lib}]}" != 1 ]]; then
            echo "Found shared library that isn't allowed: ${lib}"
            echo "Allowed: ${allowed_libraries[@]}"
            echo "Present: ${present_libraries[@]}"
            exit 1
        fi
    done

    errored=0
    while read -r symbol; do
        version=$(cut -d'@' -f2 <<<"${symbol}" | cut -d_ -f2)
        components=(${version//./ })
        if (( ${components[0]} < ${max_glibc_major_version} || (${components[0]} == ${max_glibc_major_version} && ${components[1]} < ${max_glibc_minor_version}) )); then
            continue
        fi
        errored=1
        echo "Found glibc symbol > ${max_glibc_version}: ${symbol}"
    done < <(readelf --dyn-syms --wide "${input_file}" | grep --extended-regexp --only-matching '[^ ]+@GLIBC_[^ ]+')
    if [[ "${errored}" = 1 ]]; then
        exit 1
    fi
done

echo "Portability looks good."
