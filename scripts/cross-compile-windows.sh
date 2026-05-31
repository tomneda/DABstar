#!/usr/bin/env bash
#
# Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
#
# DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2 of the License, or any later version.
#
# DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
# Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# ==============================================================================
# cross-compile-windows.sh
#
# Cross-compiles DABstar for Windows x86_64 from Ubuntu using MinGW-w64.
#
# What this script does:
#   1. Installs host build tools (apt, pip)
#   2. Downloads Qt 6 for Linux (host tools: moc/uic/rcc) via aqtinstall
#   3. Downloads Qt 6 for Windows MinGW (target libs) via aqtinstall
#   4. Cross-compiles static dependency libraries (fftw3f, zlib, libsndfile, fdk-aac)
#   5. Fetches device-SDK headers needed at compile time (rtl-sdr, etc.)
#   6. Creates a cross-targeting pkg-config wrapper
#   7. Builds DABstar with CMake + the MinGW toolchain
#   8. Assembles a deployable directory with all needed DLLs
#
# Device driver DLLs (rtlsdr.dll, etc.) are NOT included – supply them
# separately in the deploy directory at runtime, as noted in the README.
#
# Requirements:
#   Ubuntu 22.04 / 24.04 with sudo access
#   Internet connection for downloads
#
# Usage:
#   cd <DABstar-root>
#   bash scripts/cross-compile-windows.sh
# ==============================================================================

set -euo pipefail

# ------------------------------------------------------------------------------
# Configuration – edit as needed
# ------------------------------------------------------------------------------
QT_VERSION=""               # Qt version – leave empty for auto-detection of latest 6.x
JOBS=$(nproc)               # Parallel make jobs

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# All generated artefacts go here (outside the source tree):
BUILD_ROOT="${PROJECT_ROOT}/win-cross-build"

SYSROOT="${BUILD_ROOT}/sysroot"           # cross-compiled deps land here
DEPS_SRC="${BUILD_ROOT}/deps-src"         # dependency source downloads
QT_HOST_DIR="${BUILD_ROOT}/qt-host"       # Qt Linux binaries (host tools)
QT_WIN_DIR="${BUILD_ROOT}/qt-windows"     # Qt Windows MinGW binaries (target)
BUILD_DIR="${BUILD_ROOT}/dabstar-build"   # CMake build directory
DEPLOY_DIR="${BUILD_ROOT}/dabstar-deploy" # final deployable directory

TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/Toolchains/mingw-w64-x86_64.cmake"
MINGW_PREFIX="x86_64-w64-mingw32"

# Colours for output
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

# ==============================================================================
# 1. Install host tools
# ==============================================================================
install_host_tools() {
    info "Installing host build tools..."
    sudo apt-get update -q
    sudo apt-get install -y \
        mingw-w64 \
        mingw-w64-tools \
        cmake \
        ninja-build \
        pipx \
        git \
        wget \
        pkg-config

    # pipx installs CLI tools into isolated venvs – avoids the PEP 668
    # "externally-managed-environment" error on Ubuntu 22.04+ / Debian 12+.
    pipx install aqtinstall --quiet

    # Make sure pipx-installed tools are on PATH for this session
    # (~/.local/bin is added permanently by 'pipx ensurepath', but we need
    #  it right now without re-sourcing the shell profile)
    export PATH="${HOME}/.local/bin:${PATH}"
}

# ==============================================================================
# 2. Download Qt (Linux host tools + Windows target libs)
# ==============================================================================

# Detect the latest Qt 6.x version for which win64_mingw is actually available.
# NOTE: all diagnostic output goes to stderr so $() captures only the version.
_detect_qt_version() {
    echo -e "${GREEN}[INFO]${NC}  Auto-detecting latest Qt 6.x with win64_mingw support..." >&2

    # Get all 6.x versions for Windows desktop, newest last after sort -V
    local versions
    versions=$(aqt list-qt windows desktop 2>/dev/null \
               | tr ' ' '\n' | grep '^6\.' | sort -V)

    if [[ -z "${versions}" ]]; then
        echo -e "${RED}[ERROR]${NC} 'aqt list-qt windows desktop' returned nothing." >&2
        echo -e "        Check your internet connection or set QT_VERSION manually." >&2
        exit 1
    fi

    # Walk from newest to oldest and pick the first that has win64_mingw
    local ver
    for ver in $(echo "${versions}" | tac); do
        if aqt list-qt windows desktop --arch "${ver}" 2>/dev/null \
                | grep -qw "win64_mingw"; then
            echo "${ver}"   # only the version string goes to stdout
            return
        fi
        echo -e "${YELLOW}[WARN]${NC}  win64_mingw not available for ${ver}, trying older..." >&2
    done

    echo -e "${RED}[ERROR]${NC} No Qt 6.x version with win64_mingw found." >&2
    echo -e "        Set QT_VERSION manually at the top of this script." >&2
    exit 1
}

# Detect the Linux host architecture string (changed in aqtinstall ≥ 3.x).
# NOTE: all diagnostic output goes to stderr so $() captures only the arch.
_detect_linux_arch() {
    local ver="$1"
    local arch
    for arch in linux_gcc_64 gcc_64; do
        if aqt list-qt linux desktop --arch "${ver}" 2>/dev/null | grep -qw "${arch}"; then
            echo "${arch}"   # only the arch string goes to stdout
            return
        fi
    done
    echo -e "${RED}[ERROR]${NC} Could not determine Linux Qt arch for version ${ver}." >&2
    echo -e "        Debug: run 'aqt list-qt linux desktop --arch ${ver}'" >&2
    exit 1
}

download_qt() {
    # Auto-detect version if not set
    if [[ -z "${QT_VERSION}" ]]; then
        QT_VERSION="$(_detect_qt_version)"
    fi
    info "Using Qt version: ${QT_VERSION}"

    local linux_arch
    linux_arch="$(_detect_linux_arch "${QT_VERSION}")"
    info "Linux host arch: ${linux_arch}"

    # Qt addon modules required by DABstar (not part of the base package)
    local qt_modules="qtmultimedia qtcharts"

    info "Downloading Qt ${QT_VERSION} for Linux (host tools)..."
    mkdir -p "${QT_HOST_DIR}"
    if [[ ! -d "${QT_HOST_DIR}/${QT_VERSION}/${linux_arch}" ]]; then
        aqt install-qt linux desktop "${QT_VERSION}" "${linux_arch}" \
            --outputdir "${QT_HOST_DIR}" \
            --modules ${qt_modules}
    else
        info "Qt Linux host already downloaded – skipping."
    fi

    info "Downloading Qt ${QT_VERSION} for Windows MinGW (target)..."
    mkdir -p "${QT_WIN_DIR}"
    if [[ ! -d "${QT_WIN_DIR}/${QT_VERSION}/mingw_64" ]]; then
        aqt install-qt windows desktop "${QT_VERSION}" win64_mingw \
            --outputdir "${QT_WIN_DIR}" \
            --modules ${qt_modules}
    else
        info "Qt Windows target already downloaded – skipping."
    fi

    # Export for use in build_dabstar and deploy
    QT_LINUX_ARCH="${linux_arch}"
}

# ==============================================================================
# Helper: run cmake + install for a dependency into SYSROOT
# ==============================================================================
build_dep() {
    local name="$1"
    local src_dir="$2"
    shift 2
    local extra_cmake_args=("$@")

    info "Building ${name}..."
    local build_dir="${src_dir}/build-win"
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
    cmake -S "${src_dir}" -B "${build_dir}" \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
        -DCMAKE_INSTALL_PREFIX="${SYSROOT}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_FIND_ROOT_PATH="${SYSROOT}" \
        "${extra_cmake_args[@]}"
    cmake --build   "${build_dir}" --parallel "${JOBS}"
    cmake --install "${build_dir}"
    info "${name} installed."
}

# ==============================================================================
# 3. Build cross-compiled dependency libraries
# ==============================================================================
build_dependencies() {
    mkdir -p "${DEPS_SRC}"

    # --- zlib ---
    local zlib_ver="1.3.1"
    local zlib_dir="${DEPS_SRC}/zlib-${zlib_ver}"
    if [[ ! -d "${zlib_dir}" ]]; then
        wget -qO "${DEPS_SRC}/zlib.tar.gz" \
            "https://github.com/madler/zlib/releases/download/v${zlib_ver}/zlib-${zlib_ver}.tar.gz"
        tar xf "${DEPS_SRC}/zlib.tar.gz" -C "${DEPS_SRC}"
    fi
    build_dep "zlib" "${zlib_dir}"
    # zlib's cmake does not generate a .pc file – create one manually
    _write_pc_zlib "${zlib_ver}"
    # On Windows, CMake names the zlib libraries 'zlib'/'zlibstatic' instead
    # of the Unix convention 'z'. Create '-lz' compatible symlinks so the
    # DABstar linker step (-lz) finds the right library.
    local zlib_lib="${SYSROOT}/lib"
    [[ ! -f "${zlib_lib}/libz.a" ]]     && ln -sf libzlibstatic.a "${zlib_lib}/libz.a"
    [[ ! -f "${zlib_lib}/libz.dll.a" ]] && ln -sf libzlib.dll.a   "${zlib_lib}/libz.dll.a"

    # --- fftw3 (single-precision / float) ---
    local fftw_ver="3.3.10"
    local fftw_dir="${DEPS_SRC}/fftw-${fftw_ver}"
    if [[ ! -d "${fftw_dir}" ]]; then
        wget -qO "${DEPS_SRC}/fftw.tar.gz" \
            "https://www.fftw.org/fftw-${fftw_ver}.tar.gz"
        tar xf "${DEPS_SRC}/fftw.tar.gz" -C "${DEPS_SRC}"
    fi
    build_dep "fftw3f" "${fftw_dir}" \
        -DENABLE_FLOAT=ON \
        -DENABLE_SSE2=ON \
        -DDISABLE_FORTRAN=ON \
        -DWITH_OUR_MALLOC=ON \
        "-DCMAKE_C_FLAGS=-DWITH_OUR_MALLOC"

    # --- libsndfile (no external codec deps – minimal build) ---
    local sndfile_dir="${DEPS_SRC}/libsndfile"
    if [[ ! -d "${sndfile_dir}" ]]; then
        git clone --depth=1 \
            https://github.com/libsndfile/libsndfile.git \
            "${sndfile_dir}"
    fi
    build_dep "libsndfile" "${sndfile_dir}" \
        -DENABLE_EXTERNAL_LIBS=OFF \
        -DENABLE_MPEG=OFF \
        -DBUILD_PROGRAMS=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_TESTING=OFF

    # --- fdk-aac v2.0.3 ---
    local fdkaac_dir="${DEPS_SRC}/fdk-aac"
    if [[ ! -d "${fdkaac_dir}" ]]; then
        git clone --depth=1 --branch v2.0.3 \
            https://github.com/mstorsjo/fdk-aac.git \
            "${fdkaac_dir}"
    fi
    build_dep "fdk-aac" "${fdkaac_dir}"
}

# Write a minimal zlib.pc for the cross-pkg-config wrapper
_write_pc_zlib() {
    local ver="$1"
    local pc_dir="${SYSROOT}/lib/pkgconfig"
    mkdir -p "${pc_dir}"
    cat > "${pc_dir}/zlib.pc" << EOF
prefix=${SYSROOT}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: zlib
Description: zlib compression library
Version: ${ver}
Libs: -L\${libdir} -lz
Cflags: -I\${includedir}
EOF
}

# ==============================================================================
# 4. Fetch device-SDK headers (compile-time only; no link-time dependency)
#    The actual DLLs are loaded at runtime via QLibrary – users supply them.
# ==============================================================================
fetch_device_headers() {
    info "Fetching device SDK headers..."
    local inc="${SYSROOT}/include"
    mkdir -p "${inc}"

    # RTL-SDR header (from old-dab fork which DABstar recommends)
    if [[ ! -f "${inc}/rtl-sdr.h" ]]; then
        local rtlsdr_tmp="${DEPS_SRC}/rtlsdr-src"
        if [[ ! -d "${rtlsdr_tmp}" ]]; then
            git clone --depth=1 https://github.com/old-dab/rtlsdr.git "${rtlsdr_tmp}"
        fi
        cp "${rtlsdr_tmp}/include/rtl-sdr.h" "${inc}/"
        cp "${rtlsdr_tmp}/include/rtl-sdr_export.h" "${inc}/" 2>/dev/null || true

        # MinGW-w64 v9+ already provides usleep() natively, so the
        # '#define usleep(x) Sleep(x/1000)' block in this header conflicts
        # and causes a parse error with MinGW GCC 13 (win32-threads).
        # Remove the 3-line block; the toolchain's own usleep is used instead.
        sed -i '/#ifdef _WIN32$/{N;/#define usleep/{N;/^#endif$/d}}' "${inc}/rtl-sdr.h"
    fi

    # AirSpy header
    if [[ ! -f "${inc}/airspy.h" ]]; then
        local airspy_tmp="${DEPS_SRC}/airspyone-src"
        if [[ ! -d "${airspy_tmp}" ]]; then
            git clone --depth=1 https://github.com/airspy/airspyone_host.git "${airspy_tmp}"
        fi
        cp "${airspy_tmp}/libairspy/src/airspy.h"          "${inc}/" 2>/dev/null || true
        cp "${airspy_tmp}/libairspy/src/airspy_commands.h" "${inc}/" 2>/dev/null || true
    fi

    # HackRF header
    if [[ ! -f "${inc}/hackrf.h" ]]; then
        local hackrf_tmp="${DEPS_SRC}/hackrf-src"
        if [[ ! -d "${hackrf_tmp}" ]]; then
            git clone --depth=1 https://github.com/greatscottgadgets/hackrf.git "${hackrf_tmp}"
        fi
        cp "${hackrf_tmp}/host/libhackrf/src/hackrf.h" "${inc}/" 2>/dev/null || true
    fi

    info "Device headers fetched."
}

# ==============================================================================
# 5. Create a cross-targeting pkg-config wrapper
#    This makes CMake's pkg_check_modules() find the sysroot's .pc files.
# ==============================================================================
create_cross_pkgconfig() {
    local wrapper="${SYSROOT}/bin/cross-pkg-config"
    mkdir -p "${SYSROOT}/bin"
    cat > "${wrapper}" << EOF
#!/bin/bash
export PKG_CONFIG_LIBDIR="${SYSROOT}/lib/pkgconfig:${SYSROOT}/share/pkgconfig"
export PKG_CONFIG_PATH=""
export PKG_CONFIG_SYSROOT_DIR="${SYSROOT}"
exec /usr/bin/pkg-config "\$@"
EOF
    chmod +x "${wrapper}"
    info "Cross pkg-config wrapper created at ${wrapper}"
}

# ==============================================================================
# 5b. Replace Windows Qt tool .exe files with Linux shell-script wrappers.
#
# Qt's CMake integration hardcodes the path to moc/uic/rcc as <qtdir>/bin/moc.exe.
# CMake's Qt6CoreTools_DIR hint does not reliably override this for AUTOMOC.
# The fix: replace the PE binaries with #!/bin/bash wrappers that call the
# Linux equivalents – Linux's kernel executes the shebang line transparently.
# ==============================================================================
patch_qt_win_tools() {
    local qt_win_root="${QT_WIN_DIR}/${QT_VERSION}/mingw_64"
    local qt_host_root="${QT_HOST_DIR}/${QT_VERSION}/${QT_LINUX_ARCH:-linux_gcc_64}"
    local win_bin="${qt_win_root}/bin"

    info "Patching Windows Qt tool binaries with symlinks to Linux ELF tools..."

    for tool in moc uic rcc qdoc; do
        local win_exe="${win_bin}/${tool}.exe"
        # -e: exists (follows symlinks); -L: is a symlink (possibly dangling)
        [[ -e "${win_exe}" || -L "${win_exe}" ]] || continue

        # Qt ≥ 6.x moved host tools to libexec/; fall back to bin/
        local host_exe=""
        for dir in libexec bin; do
            if [[ -f "${qt_host_root}/${dir}/${tool}" ]]; then
                host_exe="${qt_host_root}/${dir}/${tool}"
                break
            fi
        done
        if [[ -z "${host_exe}" ]]; then
            warn "  Host tool not found: ${tool} – skipping"
            continue
        fi

        # Skip if already a symlink to the correct target
        if [[ -L "${win_exe}" && "$(readlink -f "${win_exe}")" == "$(readlink -f "${host_exe}")" ]]; then
            info "  Already linked: ${win_exe}"
            continue
        fi

        # Replace the Windows PE binary with a symlink to the Linux ELF binary.
        # Linux exec() handles ELF natively; the .exe extension is irrelevant.
        mv "${win_exe}" "${win_exe}.bak" 2>/dev/null || rm -f "${win_exe}"
        ln -sf "${host_exe}" "${win_exe}"
        info "  Linked: ${win_exe} → ${host_exe}"
    done
}

# ==============================================================================
# 6. Build DABstar
# ==============================================================================
build_dabstar() {
    info "Configuring DABstar for Windows..."
    mkdir -p "${BUILD_DIR}"

    local qt_win_root="${QT_WIN_DIR}/${QT_VERSION}/mingw_64"
    local qt_host_root="${QT_HOST_DIR}/${QT_VERSION}/${QT_LINUX_ARCH:-linux_gcc_64}"

    local qt_host_cmake="${qt_host_root}/lib/cmake"

    cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
        -DCMAKE_FIND_ROOT_PATH="${qt_win_root};${SYSROOT}" \
        -DCMAKE_PREFIX_PATH="${qt_win_root};${SYSROOT}" \
        -DQT_HOST_PATH="${qt_host_root}" \
        -DPKG_CONFIG_EXECUTABLE="${SYSROOT}/bin/cross-pkg-config" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCONV_IN_FILES=OFF \
        -DRTLSDR=ON \
        -DAIRSPY=OFF \
        -DHACKRF=OFF \
        -DSDRPLAY=OFF \
        -DLIMESDR=OFF \
        -DRTL_TCP=ON \
        -DPLUTO=OFF \
        -DUHD=OFF \
        -DSOAPY=OFF \
        -DFDK_AAC=ON \
        -DVITERBI_SSE2=ON \
        -DSSE_OR_AVX=OFF \
        -DUSE_LTO=OFF \
        "-DCMAKE_AUTORCC_OPTIONS=--compress-algo;zlib"

    info "Building DABstar..."
    cmake --build "${BUILD_DIR}" --parallel "${JOBS}"
    info "Build complete. Executable: ${BUILD_DIR}/dabstar.exe"
}

# ==============================================================================
# 7. Assemble a deployable directory
# ==============================================================================
deploy() {
    info "Assembling deployment directory: ${DEPLOY_DIR}"
    mkdir -p "${DEPLOY_DIR}/platforms"
    mkdir -p "${DEPLOY_DIR}/multimedia"
    mkdir -p "${DEPLOY_DIR}/sqldrivers"
    mkdir -p "${DEPLOY_DIR}/imageformats"

    # DABstar executable
    cp "${BUILD_DIR}/dabstar.exe" "${DEPLOY_DIR}/"

    local qt_win_root="${QT_WIN_DIR}/${QT_VERSION}/mingw_64"
    local qt_bin="${qt_win_root}/bin"
    local qt_plugins="${qt_win_root}/plugins"

    # --- Qt6 runtime DLLs ---
    local qt_dlls=(
        Qt6Core Qt6Gui Qt6Widgets Qt6Network Qt6Xml Qt6Sql
        Qt6Multimedia Qt6MultimediaWidgets Qt6Charts
        Qt6OpenGL Qt6OpenGLWidgets
    )
    for dll in "${qt_dlls[@]}"; do
        cp "${qt_bin}/${dll}.dll" "${DEPLOY_DIR}/" 2>/dev/null || \
            warn "Optional DLL not found: ${dll}.dll"
    done

    # Qt platform plugin (mandatory for any windowed Qt app)
    cp "${qt_plugins}/platforms/qwindows.dll"    "${DEPLOY_DIR}/platforms/"

    # Qt SQL driver (SQLite – used for service/ensemble list)
    cp "${qt_plugins}/sqldrivers/qsqlite.dll"    "${DEPLOY_DIR}/sqldrivers/" 2>/dev/null || true

    # Qt image formats
    for f in "${qt_plugins}/imageformats/"*.dll; do
        cp "$f" "${DEPLOY_DIR}/imageformats/" 2>/dev/null || true
    done

    # Qt multimedia backend
    for f in "${qt_plugins}/multimedia/"*.dll; do
        cp "$f" "${DEPLOY_DIR}/multimedia/" 2>/dev/null || true
    done

    # --- MinGW runtime DLLs ---
    # These come with the Qt Windows package
    for rt_dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
        local found
        found=$(find "${qt_win_root}" -name "${rt_dll}" 2>/dev/null | head -1)
        if [[ -n "${found}" ]]; then
            cp "${found}" "${DEPLOY_DIR}/"
        else
            warn "MinGW runtime DLL not found in Qt dir: ${rt_dll}"
            warn "  Find it in /usr/lib/gcc/${MINGW_PREFIX}/*/  and copy manually."
        fi
    done

    # --- Device driver DLLs (user-supplied) ---
    echo ""
    warn "Device driver DLLs are NOT included and must be supplied separately:"
    warn "  RTL-SDR:  librtlsdr.dll  (from https://github.com/old-dab/rtlsdr/releases)"
    warn "  HackRF:   hackrf.dll     (from https://github.com/greatscottgadgets/hackrf/releases)"
    warn "  AirSpy:   airspy.dll     (from https://github.com/airspy/airspyone_host/releases)"
    warn "  SDRplay:  sdrplay_api.dll (from https://www.sdrplay.com/api)"
    warn "Place the DLLs alongside dabstar.exe in: ${DEPLOY_DIR}/"
    echo ""

    info "Deployment directory ready: ${DEPLOY_DIR}"
}

# ==============================================================================
# Main
# ==============================================================================
main() {
    echo "============================================================"
    echo "  DABstar Windows cross-compilation (Ubuntu → MinGW-w64)"
    echo "  Qt ${QT_VERSION}  |  Build root: ${BUILD_ROOT}"
    echo "============================================================"

    install_host_tools
    download_qt
    patch_qt_win_tools
    build_dependencies
    fetch_device_headers
    create_cross_pkgconfig
    build_dabstar
    deploy

    echo ""
    echo -e "${GREEN}============================================================${NC}"
    echo -e "${GREEN}  SUCCESS – deployable Windows build in:${NC}"
    echo -e "${GREEN}  ${DEPLOY_DIR}${NC}"
    echo -e "${GREEN}============================================================${NC}"
}

main "$@"
