#!/usr/bin/env bash

# Build the native GO2/DRM frontend against the RK3326 Mali GBM blob.
#
# ArkOS/dArkOS and AmberELEC expose the unversioned EGL/GLES/GBM linker names
# through the vendor Mali library. A build made against a generic GLVND/Mesa
# development environment instead records versioned dependencies such as
# libEGL.so.1 and can select the wrong EGL implementation at runtime.

set -Eeuo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
repo_root=$(cd "${script_dir}/.." && pwd -P)

readonly mali_filename="libmali-bifrost-g31-rxp0-gbm.so"
readonly mali_source_commit="809c58cd0bf8f9dcef045d8baf380331a789836f"
readonly mali_download_url="https://raw.githubusercontent.com/christianhaitian/rk3326_core_builds/${mali_source_commit}/mali/aarch64/${mali_filename}"
readonly mali_expected_sha256="2a067ea38256c3de139b2a5396d3101f7c7777b1e19d59017b2c02172500d0ea"

mali_lib=${MALI_LIB:-}
output_path="dist/retrorun-rk3326-mali"
jobs=${JOBS:-}
max_glibc=${RK3326_MAX_GLIBC:-}
mali_cache_dir=${RK3326_MALI_CACHE_DIR:-}
download_mali=false
keep_build=false
created_links=()
temporary_output=""
temporary_download=""
clean_intermediates=false

usage() {
    cat <<'EOF'
Usage:
  tools/build-rk3326-mali.sh (--download-mali | --mali-lib PATH) [options]

Required:
  --download-mali      Download the pinned dArkOS RK3326 Mali blob, verify its
                       SHA-256 and reuse it from a persistent cache.
  --mali-lib PATH      Use an existing RK3326 AArch64 Mali GBM shared library.
                       MALI_LIB may be used instead of this option.

Options:
  --mali-cache-dir DIR Cache directory used by --download-mali. Defaults to
                       $XDG_CACHE_HOME/retrorun/rk3326-mali or
                       $HOME/.cache/retrorun/rk3326-mali.
  --output PATH        Output path relative to the repository root, or an
                       absolute path (default: dist/retrorun-rk3326-mali).
  --jobs N             Parallel make jobs (default: online CPU count).
  --max-glibc VERSION  Reject a binary requiring a newer GLIBC version.
                       RK3326_MAX_GLIBC may be used instead.
  --keep-build         Keep the intermediate retrorun binary and object files.
  -h, --help           Show this help.

The script refuses GLVND/Mesa-style versioned EGL/GLES/GBM dependencies and
prints the highest GLIBC version required by the resulting executable.
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

note() {
    printf '==> %s\n' "$*"
}

warning() {
    printf 'warning: %s\n' "$*" >&2
}

sha256_file() {
    local file_path=$1

    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "${file_path}" | sed 's/[[:space:]].*//'
    else
        shasum -a 256 "${file_path}" | sed 's/[[:space:]].*//'
    fi
}

cleanup() {
    status=$?
    trap - EXIT INT TERM

    if [[ -n "${temporary_output}" && -e "${temporary_output}" ]]; then
        rm -f -- "${temporary_output}"
    fi

    if [[ -n "${temporary_download}" && -e "${temporary_download}" ]]; then
        rm -f -- "${temporary_download}"
    fi

    if [[ ${#created_links[@]} -gt 0 ]]; then
        for link_path in "${created_links[@]}"; do
            if [[ -L "${link_path}" && "$(readlink "${link_path}")" == "${mali_lib}" ]]; then
                rm -f -- "${link_path}"
            else
                warning "leaving changed build link in place: ${link_path}"
            fi
        done
    fi

    if [[ "${clean_intermediates}" == true && "${keep_build}" == false ]]; then
        if ! make -C "${repo_root}" PLATFORM=linux-go2 config=release clean >/dev/null 2>&1; then
            warning "could not clean intermediate GO2 build files"
        fi
    fi

    exit "${status}"
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

while [[ $# -gt 0 ]]; do
    case "$1" in
        --download-mali)
            download_mali=true
            shift
            ;;
        --mali-lib)
            [[ $# -ge 2 ]] || die "--mali-lib requires a path"
            mali_lib=$2
            shift 2
            ;;
        --mali-cache-dir)
            [[ $# -ge 2 ]] || die "--mali-cache-dir requires a path"
            mali_cache_dir=$2
            shift 2
            ;;
        --output)
            [[ $# -ge 2 ]] || die "--output requires a path"
            output_path=$2
            shift 2
            ;;
        --jobs)
            [[ $# -ge 2 ]] || die "--jobs requires a positive integer"
            jobs=$2
            shift 2
            ;;
        --max-glibc)
            [[ $# -ge 2 ]] || die "--max-glibc requires a version"
            max_glibc=$2
            shift 2
            ;;
        --keep-build)
            keep_build=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown argument: $1"
            ;;
    esac
done

[[ "$(uname -s)" == "Linux" ]] || die "this GO2 build must run in a Linux target or cross-build environment"

for tool in make readelf sed grep sort install mktemp mv ln readlink; do
    command -v "${tool}" >/dev/null 2>&1 || die "required tool not found: ${tool}"
done

if ! command -v sha256sum >/dev/null 2>&1 && ! command -v shasum >/dev/null 2>&1; then
    die "required SHA-256 tool not found: install sha256sum or shasum"
fi

if [[ "${download_mali}" == true && -n "${mali_lib}" ]]; then
    die "use either --download-mali or --mali-lib/MALI_LIB, not both"
fi

if [[ "${download_mali}" == true ]]; then
    command -v curl >/dev/null 2>&1 || die "--download-mali requires curl"

    if [[ -z "${mali_cache_dir}" ]]; then
        cache_base=${XDG_CACHE_HOME:-${HOME:-}}
        [[ -n "${cache_base}" ]] || die "cannot determine a cache directory; use --mali-cache-dir"
        if [[ -n "${XDG_CACHE_HOME:-}" ]]; then
            mali_cache_dir="${cache_base}/retrorun/rk3326-mali"
        else
            mali_cache_dir="${cache_base}/.cache/retrorun/rk3326-mali"
        fi
    fi

    mkdir -p -- "${mali_cache_dir}"
    mali_cache_dir=$(cd "${mali_cache_dir}" && pwd -P)
    cached_mali="${mali_cache_dir}/${mali_filename}"

    cached_sha256=""
    if [[ -f "${cached_mali}" ]]; then
        cached_sha256=$(sha256_file "${cached_mali}")
    fi

    if [[ "${cached_sha256}" == "${mali_expected_sha256}" ]]; then
        note "Using verified cached Mali library: ${cached_mali}"
    else
        if [[ -n "${cached_sha256}" ]]; then
            warning "cached Mali library has an invalid SHA-256 and will be replaced after a verified download"
        fi

        note "Downloading pinned RK3326 Mali library"
        note "Source commit: ${mali_source_commit}"
        temporary_download=$(mktemp "${mali_cache_dir}/.${mali_filename}.XXXXXX")
        curl --fail --location --retry 3 --connect-timeout 15 \
            --output "${temporary_download}" "${mali_download_url}"

        downloaded_sha256=$(sha256_file "${temporary_download}")
        [[ "${downloaded_sha256}" == "${mali_expected_sha256}" ]] || \
            die "downloaded Mali SHA-256 mismatch: expected ${mali_expected_sha256}, got ${downloaded_sha256}"

        install -m 0644 "${temporary_download}" "${cached_mali}"
        rm -f -- "${temporary_download}"
        temporary_download=""
        note "Cached verified Mali library: ${cached_mali}"
    fi

    mali_lib=${cached_mali}
fi

[[ -n "${mali_lib}" ]] || die "provide --download-mali, --mali-lib PATH or MALI_LIB"
[[ -f "${mali_lib}" ]] || die "Mali library not found: ${mali_lib}"

mali_dir=$(cd "$(dirname "${mali_lib}")" && pwd -P)
mali_lib="${mali_dir}/$(basename "${mali_lib}")"

if [[ -z "${jobs}" ]]; then
    jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')
fi
[[ "${jobs}" =~ ^[1-9][0-9]*$ ]] || die "--jobs must be a positive integer"

if [[ -n "${max_glibc}" && ! "${max_glibc}" =~ ^[0-9]+([.][0-9]+)+$ ]]; then
    die "--max-glibc must look like 2.31"
fi

machine=$(LC_ALL=C readelf -h "${mali_lib}" | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p')
[[ "${machine}" == *AArch64* ]] || die "Mali library is not AArch64 (ELF machine: ${machine:-unknown})"

if [[ "${output_path}" != /* ]]; then
    output_path="${repo_root}/${output_path}"
fi
[[ "${output_path}" != "${repo_root}/retrorun" ]] || die "--output cannot overwrite the intermediate repository binary"

output_dir=$(dirname "${output_path}")
mkdir -p -- "${output_dir}"

link_names=(libEGL.so libGLESv2.so libgbm.so)
for link_name in "${link_names[@]}"; do
    link_path="${repo_root}/${link_name}"
    if [[ -e "${link_path}" || -L "${link_path}" ]]; then
        die "refusing to replace existing build input: ${link_path}"
    fi
done

note "Mali library: ${mali_lib}"
note "Cleaning previous GO2 build"
make -C "${repo_root}" PLATFORM=linux-go2 config=release clean
clean_intermediates=true

note "Installing temporary Mali linker names"
for link_name in "${link_names[@]}"; do
    link_path="${repo_root}/${link_name}"
    ln -s -- "${mali_lib}" "${link_path}"
    created_links+=("${link_path}")
done

note "Building RetroRun for RK3326 with ${jobs} job(s)"
make -C "${repo_root}" PLATFORM=linux-go2 config=release -j"${jobs}"

binary="${repo_root}/retrorun"
[[ -x "${binary}" ]] || die "build completed without producing an executable: ${binary}"

dynamic_info=$(LC_ALL=C readelf -d "${binary}")
mapfile -t needed < <(printf '%s\n' "${dynamic_info}" | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p')
[[ ${#needed[@]} -gt 0 ]] || die "could not read DT_NEEDED entries from ${binary}"

note "Validating dynamic dependencies"
printf '    %s\n' "${needed[@]}"

has_needed() {
    local expected=$1
    printf '%s\n' "${needed[@]}" | grep -Fqx -- "${expected}"
}

vendor_provider=false
for provider in libEGL.so libmali.so libmali.so.1 libMali.so; do
    if has_needed "${provider}"; then
        vendor_provider=true
        break
    fi
done
[[ "${vendor_provider}" == true ]] || die "binary has no unversioned EGL/Mali provider dependency"

for forbidden in libEGL.so.1 libEGL_mesa.so.0 libGLESv2.so.2 libgbm.so.1 libGLdispatch.so.0; do
    if has_needed "${forbidden}"; then
        die "forbidden GLVND/Mesa-style dependency detected: ${forbidden}"
    fi
done

glibc_names=$(LC_ALL=C readelf --version-info "${binary}" 2>/dev/null | grep -Eo 'GLIBC_[0-9]+([.][0-9]+)+' || true)
highest_glibc=""
if [[ -n "${glibc_names}" ]]; then
    highest_glibc=$(printf '%s\n' "${glibc_names}" | sed 's/^GLIBC_//' | sort -Vu | tail -n 1)
    note "Highest required GLIBC version: ${highest_glibc}"
fi

if [[ -n "${max_glibc}" && -n "${highest_glibc}" ]]; then
    newer=$(printf '%s\n%s\n' "${max_glibc}" "${highest_glibc}" | sort -Vu | tail -n 1)
    [[ "${newer}" == "${max_glibc}" ]] || die "binary requires GLIBC ${highest_glibc}, newer than allowed ${max_glibc}"
fi

temporary_output=$(mktemp "${output_dir}/.retrorun-rk3326-mali.XXXXXX")
install -m 0755 "${binary}" "${temporary_output}"
mv -f -- "${temporary_output}" "${output_path}"
temporary_output=""

checksum=$(sha256_file "${output_path}")

note "Build ready: ${output_path}"
note "SHA-256: ${checksum}"

if [[ "${keep_build}" == true ]]; then
    warning "--keep-build selected; intermediate objects and ${binary} remain in the checkout"
fi
