print_help() {
    cat <<EOF
Options:
  --build-dir <path>
  --help
  --list
  --only-download
EOF
}

find_root_dir() {
    local dir=${1}
    while true; do
        if [[ -z "${dir}" || "${dir}" == "/" ]]; then
            return 1
        fi
        if [[ -f "${dir}/CMakeLists.txt" ]]; then
            break
        fi
        dir=$(dirname "${dir}")
    done
    echo "${dir}"
}

is_dep_state_flag_set() {
    local name=${1}
    local flag=${2}
    local flag_file=${dep_state_state_dir[${name}]}/${flag}
    if [[ -f "${flag_file}" ]]; then
        return 0
    fi
    return 1
}

set_dep_state_flag() {
    local name=${1}
    local flag=${2}
    local flag_file=${dep_state_state_dir[${name}]}/${flag}
    mkdir -p "$(dirname "${flag_file}")"
    touch "${flag_file}"
}

is_7z_archive() {
    [[ "${1}" =~ \.7z$ ]] || return 1
}

is_tar_archive() {
    [[ "${1}" =~ \.tar\. ]] || return 1
}

get_install_prefix() {
    echo "${deps_install_dir}"
}

list_all_deps() {
    for name in "${dep_state_names[@]}"; do
        local version=${dep_state_versions[${name}]}
        echo "${name} ${version}"
    done | sort
}

dep_init() {
    while (( $# > 0 )); do
        local arg_name=${1}
        shift
        local arg_value=${1}
        shift
        case "${arg_name}" in
            --cmake-arg)
                cmake_common_configure_args+=("${arg_value}")
                ;;
            *)
                echo "Invalid argument: ${arg_name}"
                return 1
                ;;
        esac
    done
}

dep_add() {
    local cmake_args=()
    local build_targets=()
    while (( $# > 0 )); do
        local arg_name=${1}
        shift
        local arg_value=${1}
        shift
        case "${arg_name}" in
            --name)
                local name=${arg_value}
                ;;
            --version)
                local version=${arg_value}
                ;;
            --hash-sha256)
                local hash_sha256=${arg_value}
                ;;
            --url)
                local url=${arg_value}
                ;;
            --subdir)
                local source_subdir=${arg_value}
                ;;
            --cmake-arg)
                cmake_args+=("${arg_value}")
                ;;
            --build-target)
                build_targets+=("${arg_value}")
                ;;
            *)
                echo "Invalid argument: ${arg_name}"
                return 1
                ;;
        esac
    done

    local state_dir=${deps_state_dir}/${name}/${version}
    local local_archive=${deps_download_dir}/${name}/${version}/${url##*/}
    local source_dir=${deps_source_dir}/${name}/${version}
    local build_dir=${deps_build_dir}/${name}/${version}
    local patch_file=${patches_dir}/${name}_${version}.patch

    dep_state_names+=(${name})
    dep_state_versions[${name}]=${version}
    dep_state_hash_sha256s[${name}]=${hash_sha256}
    dep_state_urls[${name}]=${url}
    dep_state_state_dir[${name}]=${state_dir}
    dep_state_local_archives[${name}]=${local_archive}
    dep_state_source_dirs[${name}]=${source_dir}
    dep_state_source_subdirs[${name}]=${source_subdir}
    dep_state_build_dirs[${name}]=${build_dir}

    if [[ -f "${patch_file}" ]]; then
        dep_state_patch_file[${name}]=${patch_file}
    fi

    dep_state_build_targets[${name}]=${build_targets}

    local var_safe_name=${name//-/_}
    declare -ga "dep_state_cmake_args_${var_safe_name}"
    declare -n dep_state_cmake_args="dep_state_cmake_args_${var_safe_name}"
    dep_state_cmake_args+=("${cmake_args[@]}")
}

dep_download() {
    local name=${1}
    local hash_sha256=${dep_state_hash_sha256s[${name}]}
    local url=${dep_state_urls[${name}]}
    local local_archive=${dep_state_local_archives[${name}]}
    if ! is_dep_state_flag_set "${name}" downloaded; then
        echo "Download: ${name}"
        if [[ -f "${local_archive}" ]]; then
            rm -f "${local_archive}"
        fi
        mkdir -p "$(dirname "${local_archive}")"
        curl -fsSLo "${local_archive}" "${url}"
        echo "${hash_sha256} ${local_archive}" | sha256sum --check --strict --status || (echo "Computed checksum did not match."; return 1)
        set_dep_state_flag "${name}" downloaded
    fi
}

dep_extract() {
    local name=${1}
    local local_archive=${dep_state_local_archives[${name}]}
    local source_dir=${dep_state_source_dirs[${name}]}
    if ! is_dep_state_flag_set "${name}" extracted; then
        echo "Extract: ${name}"
        if [[ -d "${source_dir}" ]]; then
            rm -rf "${source_dir}"
        fi
        mkdir -p "${source_dir}"
        if is_7z_archive "${local_archive}"; then
            7z x "${local_archive}" "-o${source_dir}" -bso0 -bsp0
        elif is_tar_archive "${local_archive}"; then
            tar --extract --file "${local_archive}" --directory "${source_dir}"
        else
            echo "Unknown archive type: ${local_archive}"
            return 1
        fi
        set_dep_state_flag "${name}" extracted
    fi
}

dep_patch() {
    local name=${1}
    local version=${dep_state_versions[${name}]}
    local source_dir=${dep_state_source_dirs[${name}]}/${dep_state_source_subdirs[${name}]}
    local patch_file=${dep_state_patch_file[${name}]}
    if [[ -z "${patch_file}" ]]; then
        return 0
    fi
    if ! is_dep_state_flag_set "${name}" patched; then
        echo "Patch: ${name}"
        patch --batch --unified --strip 1 --directory "${source_dir}" --input "${patch_file}"
        set_dep_state_flag "${name}" patched
    fi
}

dep_build() {
    local name=${1}
    shift

    local var_safe_name=${name//-/_}
    declare -n cmake_configure_args="dep_state_cmake_args_${var_safe_name}"

    cmake_configure_args+=("${cmake_common_configure_args[@]}")
    local source_dir=${dep_state_source_dirs[${name}]}/${dep_state_source_subdirs[${name}]}
    local build_dir=${dep_state_build_dirs[${name}]}
    local build_target_args=()
    local build_targets=(${dep_state_build_targets[${name}]})
    local target_count=${#build_targets[@]}
    local target_index=0
    while (( ${target_index} < ${target_count} )); do
        local target=${build_targets[${target_index}]}
        build_target_args+=(--target "${target}")
        (( ++target_index ))
    done
    if ! is_dep_state_flag_set "${name}" built; then
        echo "Build: ${name}"
        mkdir -p "${build_dir}"
        cmake -G Ninja -B "${build_dir}" -S "${source_dir}" "${cmake_configure_args[@]}"
        cmake --build "${build_dir}" "${build_target_args[@]}"
        cmake --install "${build_dir}" --prefix "${deps_install_dir}"
        set_dep_state_flag "${name}" built
    fi
}

dep_commit() {
    if [[ "${main_args["--list"]}" == 1 ]]; then
        list_all_deps
        return 0
    fi
    for name in "${dep_state_names[@]}"; do
        dep_download "${name}"
    done
    if [[ "${main_args["--only-download"]}" == 1 ]]; then
        return 0
    fi
    for name in "${dep_state_names[@]}"; do
        dep_extract "${name}"
        dep_patch "${name}"
        dep_build "${name}"
    done
}

parse_main_args() {
    while (( $# > 0 )); do
        local arg_name=${1}
        shift
        local is_flag=0
        case "${arg_name}" in
            --build-dir)
                ;;
            --help)
                print_help
                return 1
                ;;
            --list)
                is_flag=1
                ;;
            --only-download)
                is_flag=1
                ;;
            *)
                echo "Invalid argument: ${arg_name}"
                print_help
                return 1
                ;;
        esac
        if [[ "${is_flag}" == 1 ]]; then
            local arg_value=1
        else
            local arg_value=${1}
            shift
        fi
        main_args[${arg_name}]=${arg_value}
    done
}

declare -A main_args
parse_main_args "${@}"

script_dir=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
root_dir=$(find_root_dir "${script_dir}")

desired_build_dir=${main_args["--build-dir"]}
if [[ -z "${desired_build_dir}" ]]; then
    build_dir=${root_dir}/build
else
    build_dir=${desired_build_dir}
fi
build_dir=$(realpath "${build_dir}")

patches_dir=${script_dir}/patches
deps_dir=${build_dir}/deps
deps_state_dir=${deps_dir}/state
deps_download_dir=${deps_dir}/download
deps_source_dir=${deps_dir}/source
deps_build_dir=${deps_dir}/build
deps_install_dir=${deps_dir}/install

dep_state_names=()
declare -A dep_state_versions
declare -A dep_state_hash_sha256s
declare -A dep_state_urls
declare -A dep_state_state_dir
declare -A dep_state_local_archives
declare -A dep_state_source_dirs
declare -A dep_state_source_subdirs
declare -A dep_state_build_dirs
declare -A dep_state_patch_file
declare -A dep_state_build_targets

cmake_common_configure_args=(
    -DCMAKE_PREFIX_PATH="${deps_install_dir}"
)
