#!/usr/bin/env bash
set -euo pipefail

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

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
    pass_msg='  \e[32m✓ PASS\e[0m\n'
    fail_msg='  \e[31m✗ FAIL\e[0m\n'
else
    pass_msg='  ✓ PASS\n'
    fail_msg='  ✗ FAIL\n'
fi

if (( $# == 0 )); then
    echo "No input files specified."
    exit 1
fi

failed=0
for input_file in "$@"; do
    echo "Will check portability of file: ${input_file}"

    echo '• Checking shared libraries.'
    present_libraries=($(readelf --dynamic "${input_file}" | grep NEEDED | cut -d[ -f2 | cut -d] -f1 | sort))
    for lib in "${present_libraries[@]}"; do
        if [[ "${allowed_libraries_map[${lib}]:-}" != 1 ]]; then
            failed=1
            echo "Found shared library that isn't allowed: ${lib}"
        fi
    done
    if [[ "${failed}" == 1 ]]; then
        echo "Only these libraries are allowed: ${allowed_libraries[@]}"
        printf '%b' "${fail_msg}"
        break
    else
        printf '%b' "${pass_msg}"
    fi

    echo '• Checking glibc symbols.'
    while read -r symbol; do
        version=$(cut -d'@' -f2 <<<"${symbol}" | cut -d_ -f2)
        components=(${version//./ })
        if (( ${components[0]} < ${max_glibc_major_version} || (${components[0]} == ${max_glibc_major_version} && ${components[1]} <= ${max_glibc_minor_version}) )); then
            continue
        fi
        failed=1
        echo "Found glibc symbol > ${max_glibc_version}: ${symbol}"
    done < <(readelf --dyn-syms --wide "${input_file}" | grep --extended-regexp --only-matching '[^ ]+@GLIBC_[^ ]+')
    if [[ "${failed}" == 1 ]]; then
        printf '%b' "${fail_msg}"
        break
    else
        printf '%b' "${pass_msg}"
    fi
done

if [[ "${failed}" == 1 ]]; then
    exit 1
fi
