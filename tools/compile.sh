#!/bin/bash
set -e

#=============================================================================
# 项目编译脚本
# 用法:
#   ./compile.sh [选项] [组件...]
#
# 选项:
#   --arch=ARCH         目标架构: x86_64(默认) 或 aarch64
#   --cross=PREFIX      交叉编译工具链前缀 (如 aarch64-linux-gnu-)
#   --sysroot=PATH      交叉编译 sysroot 路径
#   --jobs=N            并行编译线程数 (默认: nproc)
#   --clean             编译前清理 build 目录
#   --help              帮助信息
#
# 组件 (不指定则编译全部):
#   mpp x264 x265 libdrm ffmpeg opencv librga rknn
#   m4 autoconf libtool bison flex openssl gsoap all
#
# 示例:
#   ./compile.sh                          # x86_64 编译全部
#   ./compile.sh --arch=aarch64           # 交叉编译全部 (使用默认 aarch64-linux-gnu-)
#   ./compile.sh --arch=aarch64 mpp x264  # 交叉编译 mpp 和 x264
#   ./compile.sh --clean ffmpeg           # 清理后重新编译 ffmpeg
#=============================================================================

# ==================== 颜色输出 ====================
RED='\e[31m'
GREEN='\e[32m'
YELLOW='\e[33m'
BLUE='\e[34m'
RESET='\e[0m'

log_info()  { echo -e "${BLUE}[INFO]${RESET}  $*"; }
log_ok()    { echo -e "${GREEN}[OK]${RESET}    $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
log_error() { echo -e "${RED}[ERROR]${RESET} $*"; }

# ==================== 默认配置 ====================
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
ARCH="x86_64"
CROSS_PREFIX=""
SYSROOT=""
JOBS=$(nproc)
CLEAN=0
COMPONENTS=()

# ==================== 解析参数 ====================
show_help() {
    sed -n '2,/^#=====/p' "$0" | grep '^#' | sed 's/^# \?//'
    exit 0
}

for arg in "$@"; do
    case "$arg" in
        --arch=*)       ARCH="${arg#*=}" ;;
        --cross=*)      CROSS_PREFIX="${arg#*=}" ;;
        --sysroot=*)    SYSROOT="${arg#*=}" ;;
        --jobs=*)       JOBS="${arg#*=}" ;;
        --clean)        CLEAN=1 ;;
        --help|-h)      show_help ;;
        -*)             log_error "未知选项: $arg"; show_help ;;
        *)              COMPONENTS+=("$arg") ;;
    esac
done

# 如果是 aarch64 但未指定 cross prefix，使用默认值
if [[ "$ARCH" == "aarch64" && -z "$CROSS_PREFIX" ]]; then
    CROSS_PREFIX="aarch64-linux-gnu-"
fi

# 安装路径: output/<arch>/
INSTALL_PREFIX="${SCRIPT_DIR}/output/${ARCH}"
BUILD_BASE="${SCRIPT_DIR}/build/${ARCH}"

mkdir -p "$INSTALL_PREFIX" "$BUILD_BASE"

# 设置 PKG_CONFIG_PATH，让后续组件能找到前面编译的库
export PKG_CONFIG_PATH="${INSTALL_PREFIX}/lib/pkgconfig:${INSTALL_PREFIX}/lib64/pkgconfig:${PKG_CONFIG_PATH}"

# ==================== 工具函数 ====================

# 检查组件是否需要编译
should_build() {
    local name="$1"
    if [[ ${#COMPONENTS[@]} -eq 0 ]] || [[ " ${COMPONENTS[*]} " =~ " all " ]] || [[ " ${COMPONENTS[*]} " =~ " $name " ]]; then
        return 0
    fi
    return 1
}

# 准备 build 目录
prepare_build_dir() {
    local name="$1"
    local build_dir="${BUILD_BASE}/${name}"
    if [[ "$CLEAN" -eq 1 ]] && [[ -d "$build_dir" ]]; then
        log_warn "清理 ${name} build 目录..."
        rm -rf "$build_dir"
    fi
    mkdir -p "$build_dir"
    echo "$build_dir"
}

# 生成 CMake 交叉编译工具链文件
generate_cmake_toolchain() {
    local toolchain_file="${BUILD_BASE}/toolchain.cmake"
    if [[ "$ARCH" == "aarch64" ]]; then
        cat > "$toolchain_file" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER ${CROSS_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_PREFIX}g++)
set(CMAKE_FIND_ROOT_PATH ${INSTALL_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
        if [[ -n "$SYSROOT" ]]; then
            echo "set(CMAKE_SYSROOT ${SYSROOT})" >> "$toolchain_file"
        fi
    fi
    echo "$toolchain_file"
}

# 通用 cmake 参数
cmake_common_args() {
    local args=(
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
    )
    if [[ "$ARCH" == "aarch64" ]]; then
        args+=(-DCMAKE_TOOLCHAIN_FILE="$(generate_cmake_toolchain)")
    fi
    echo "${args[@]}"
}

# ==================== 预编译库安装函数 ====================

# librga: 直接复制预编译库和头文件到 output 目录
install_librga() {
    local src="${SRC_DIR}/librga"

    log_info "安装 librga (预编译库) ..."

    # 根据架构选择库目录
    local lib_dir
    case "$ARCH" in
        aarch64) lib_dir="${src}/libs/Linux/gcc-aarch64" ;;
        armhf)   lib_dir="${src}/libs/Linux/gcc-armhf" ;;
        *)       log_error "librga 不支持架构: ${ARCH} (仅提供 aarch64/armhf 预编译库)"; return 1 ;;
    esac

    if [[ ! -d "$lib_dir" ]]; then
        log_error "librga 库目录不存在: ${lib_dir}"
        return 1
    fi

    # 复制库文件
    mkdir -p "${INSTALL_PREFIX}/lib"
    cp -a "${lib_dir}"/*.so "${INSTALL_PREFIX}/lib/" 2>/dev/null || true
    cp -a "${lib_dir}"/*.a  "${INSTALL_PREFIX}/lib/" 2>/dev/null || true

    # 复制头文件
    mkdir -p "${INSTALL_PREFIX}/include/rga"
    cp -a "${src}/include/"*.h "${INSTALL_PREFIX}/include/rga/"

    log_ok "librga 安装完成"
}

# rknn: 直接复制预编译库和头文件到 output 目录
install_rknn() {
    local src="${SRC_DIR}/rknn-toolkit2/rknpu2/runtime/Linux/librknn_api"

    log_info "安装 RKNN Runtime (预编译库) ..."

    # 根据架构选择库目录
    local lib_dir
    case "$ARCH" in
        aarch64) lib_dir="${src}/aarch64" ;;
        armhf)   lib_dir="${src}/armhf" ;;
        *)       log_error "RKNN 不支持架构: ${ARCH} (仅提供 aarch64/armhf 预编译库)"; return 1 ;;
    esac

    if [[ ! -d "$lib_dir" ]]; then
        log_error "RKNN 库目录不存在: ${lib_dir}"
        return 1
    fi

    # 复制库文件
    mkdir -p "${INSTALL_PREFIX}/lib"
    cp -a "${lib_dir}"/*.so "${INSTALL_PREFIX}/lib/" 2>/dev/null || true

    # 复制头文件
    mkdir -p "${INSTALL_PREFIX}/include/rknn"
    cp -a "${src}/include/"*.h "${INSTALL_PREFIX}/include/rknn/"

    log_ok "RKNN Runtime 安装完成"
}

# ==================== 组件编译函数 ====================

build_mpp() {
    local src="${SRC_DIR}/mpp"
    local build_dir
    build_dir=$(prepare_build_dir mpp)

    log_info "编译 MPP ..."

    cd "$build_dir"
    if [[ "$ARCH" == "aarch64" ]]; then
        cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
            -DCMAKE_TOOLCHAIN_FILE="$(generate_cmake_toolchain)" \
            -DRKPLATFORM=ON \
            -G "Unix Makefiles" \
            "$src"
    else
        cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
            -G "Unix Makefiles" \
            "$src"
    fi

    make -j"$JOBS"
    make install
    cd "$SCRIPT_DIR"
    log_ok "MPP 编译完成"
}

build_x264() {
    local src="${SRC_DIR}/x264"
    local build_dir
    build_dir=$(prepare_build_dir x264)

    log_info "编译 x264 ..."

    cd "$build_dir"
    local config_args=(
        --prefix="${INSTALL_PREFIX}"
        --enable-shared
        --enable-static
        --enable-pic
    )

    if [[ "$ARCH" == "aarch64" ]]; then
        config_args+=(
            --host=aarch64-linux
            --cross-prefix="${CROSS_PREFIX}"
        )
        if [[ -n "$SYSROOT" ]]; then
            config_args+=(--sysroot="${SYSROOT}")
        fi
    fi

    "$src"/configure "${config_args[@]}"
    make -j"$JOBS"
    make install
    cd "$SCRIPT_DIR"
    log_ok "x264 编译完成"
}

build_x265() {
    local src="${SRC_DIR}/x265_4.1/source"
    local build_dir
    build_dir=$(prepare_build_dir x265)

    log_info "编译 x265 ..."

    cd "$build_dir"
    local cmake_args=(
        -Wno-dev
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
        -DENABLE_SHARED=ON
        -DENABLE_SVE2=OFF
        -DENABLE_SVE=OFF
        -G "Unix Makefiles"
    )

    if [[ "$ARCH" == "aarch64" ]]; then
        cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="$(generate_cmake_toolchain)")
    fi

    cmake "${cmake_args[@]}" "$src"
    make -j"$JOBS"
    make install
    cd "$SCRIPT_DIR"
    log_ok "x265 编译完成"
}

build_libdrm() {
    local src="${SRC_DIR}/libdrm-2.4.123"
    local build_dir
    build_dir=$(prepare_build_dir libdrm)

    log_info "编译 libdrm ..."

    cd "$build_dir"
    local meson_args=(
        --prefix="${INSTALL_PREFIX}"
        --buildtype=release
        --default-library=both
        -Dintel=disabled
        -Dradeon=disabled
        -Damdgpu=disabled
        -Dnouveau=disabled
        -Dvmwgfx=disabled
        -Dtests=false
        -Dman-pages=disabled
        -Dvalgrind=disabled
    )

    if [[ "$ARCH" == "aarch64" ]]; then
        # 生成 meson 交叉编译配置文件
        local cross_file="${BUILD_BASE}/meson-cross-aarch64.ini"
        cat > "$cross_file" <<MEOF
[binaries]
c = '${CROSS_PREFIX}gcc'
cpp = '${CROSS_PREFIX}g++'
ar = '${CROSS_PREFIX}ar'
strip = '${CROSS_PREFIX}strip'
pkgconfig = 'pkg-config'

[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
MEOF
        meson_args+=(--cross-file "$cross_file")
    fi

    # meson setup 只在首次执行，已存在则跳过
    if [[ ! -f "$build_dir/build.ninja" ]]; then
        meson setup "$build_dir" "$src" "${meson_args[@]}"
    fi
    ninja -C "$build_dir" -j"$JOBS"
    ninja -C "$build_dir" install
    cd "$SCRIPT_DIR"
    log_ok "libdrm 编译完成"
}

build_ffmpeg() {
    local src="${SRC_DIR}/ffmpeg"
    local build_dir
    build_dir=$(prepare_build_dir ffmpeg)

    log_info "编译 FFmpeg ..."

    cd "$build_dir"
    local config_args=(
        --prefix="${INSTALL_PREFIX}"
        --enable-gpl
        --enable-nonfree
        --enable-version3
        --enable-libx264
        --enable-libx265
        --pkg-config-flags="--static"
        --extra-cflags="-I${INSTALL_PREFIX}/include"
        --extra-ldflags="-L${INSTALL_PREFIX}/lib -L${INSTALL_PREFIX}/lib64"
    )

    if [[ "$ARCH" == "aarch64" ]]; then
        config_args+=(
            --arch=aarch64
            --target-os=linux
            --cross-prefix="${CROSS_PREFIX}"
            --pkg-config=pkg-config
            --enable-rkmpp
            --enable-libdrm
        )
        if [[ -n "$SYSROOT" ]]; then
            config_args+=(--sysroot="${SYSROOT}")
        fi
    fi

    "$src"/configure "${config_args[@]}"
    make -j"$JOBS"
    make install
    cd "$SCRIPT_DIR"
    log_ok "FFmpeg 编译完成"
}

build_opencv() {
    local src="${SRC_DIR}/opencv-4.13.0"
    local build_dir
    build_dir=$(prepare_build_dir opencv)

    log_info "编译 OpenCV ..."

    cd "$build_dir"
    local cmake_args=(
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
        -DBUILD_SHARED_LIBS=ON
        -DWITH_FFMPEG=ON
        -DWITH_GTK=OFF
        -DWITH_QT=OFF
        -DBUILD_TESTS=OFF
        -DBUILD_PERF_TESTS=OFF
        -DBUILD_EXAMPLES=OFF
        -DBUILD_opencv_python3=OFF
        -G "Unix Makefiles"
    )

    if [[ "$ARCH" == "aarch64" ]]; then
        cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="$(generate_cmake_toolchain)")
    fi

    cmake "${cmake_args[@]}" "$src"
    make -j"$JOBS"
    make install
    cd "$SCRIPT_DIR"
    log_ok "OpenCV 编译完成"
}

# ==================== 工具链/基础库编译函数 ====================

# 通用 autotools 交叉编译函数
# 参数: $1=组件名, $2=源码目录, $3...额外 configure 参数
autotools_build() {
    local name="$1"
    local src="$2"
    shift 2
    local extra_args=("$@")

    local build_dir
    build_dir=$(prepare_build_dir "$name")

    log_info "编译 ${name} ..."

    cd "$build_dir"
    local config_args=(
        --prefix="${INSTALL_PREFIX}"
    )

    if [[ "$ARCH" == "aarch64" ]]; then
        config_args+=(
            --host=aarch64-linux-gnu
            CC="${CROSS_PREFIX}gcc"
            CXX="${CROSS_PREFIX}g++"
            AR="${CROSS_PREFIX}ar"
            RANLIB="${CROSS_PREFIX}ranlib"
            STRIP="${CROSS_PREFIX}strip"
        )
        if [[ -n "$SYSROOT" ]]; then
            config_args+=(--with-sysroot="${SYSROOT}")
        fi
    fi

    config_args+=("${extra_args[@]}")

    "$src"/configure "${config_args[@]}"
    make -j"$JOBS"
    make install
    cd "$SCRIPT_DIR"
    log_ok "${name} 编译完成"
}

build_m4() {
    autotools_build m4 "${SRC_DIR}/m4-1.4.19"
}

build_autoconf() {
    autotools_build autoconf "${SRC_DIR}/autoconf-2.72"
}

build_libtool() {
    autotools_build libtool "${SRC_DIR}/libtool-2.5.4"
}

build_bison() {
    autotools_build bison "${SRC_DIR}/bison-3.8.2"
}

build_flex() {
    autotools_build flex "${SRC_DIR}/flex-2.6.4"
}

build_openssl() {
    local src="${SRC_DIR}/openssl-3.4.1"
    local build_dir
    build_dir=$(prepare_build_dir openssl)

    log_info "编译 OpenSSL ..."

    cd "$build_dir"
    local config_args=(
        --prefix="${INSTALL_PREFIX}"
        --openssldir="${INSTALL_PREFIX}/ssl"
        shared
    )

    if [[ "$ARCH" == "aarch64" ]]; then
        config_args=(
            linux-aarch64
            --cross-compile-prefix="${CROSS_PREFIX}"
            "${config_args[@]}"
        )
    fi

    "$src"/Configure "${config_args[@]}"
    make -j"$JOBS"
    make install_sw
    cd "$SCRIPT_DIR"
    log_ok "OpenSSL 编译完成"
}

build_gsoap() {
    local src="${SRC_DIR}/gsoap-2.8"
    local build_dir
    build_dir=$(prepare_build_dir gsoap)

    log_info "编译 gSOAP ..."

    if [[ "$ARCH" == "aarch64" ]]; then
        # gsoap 交叉编译分两步:
        # 1) 先编译本机版工具 (soapcpp2, wsdl2h)
        local native_dir="${BUILD_BASE}/gsoap-native"
        if [[ ! -f "$native_dir/gsoap/src/soapcpp2" ]]; then
            log_info "  编译 gSOAP 本机工具 (soapcpp2/wsdl2h) ..."
            mkdir -p "$native_dir"
            cd "$native_dir"
            "$src"/configure --prefix="${native_dir}/install"
            make -j"$JOBS" -C gsoap/src
            cd "$SCRIPT_DIR"
        fi

        # 2) 交叉编译 gsoap 库
        cd "$build_dir"
        "$src"/configure \
            --prefix="${INSTALL_PREFIX}" \
            --host=aarch64-linux-gnu \
            CC="${CROSS_PREFIX}gcc" \
            CXX="${CROSS_PREFIX}g++" \
            AR="${CROSS_PREFIX}ar" \
            RANLIB="${CROSS_PREFIX}ranlib" \
            SOAPCPP2="${native_dir}/gsoap/src/soapcpp2" \
            WSDL2H="${native_dir}/gsoap/wsdl/wsdl2h"
        make -j"$JOBS"
        make install
    else
        cd "$build_dir"
        "$src"/configure --prefix="${INSTALL_PREFIX}"
        make -j"$JOBS"
        make install
    fi

    cd "$SCRIPT_DIR"
    log_ok "gSOAP 编译完成"
}

# ==================== 主流程 ====================
echo "========================================"
log_info "目标架构:   ${ARCH}"
log_info "安装路径:   ${INSTALL_PREFIX}"
log_info "编译线程:   ${JOBS}"
if [[ "$ARCH" == "aarch64" ]]; then
    log_info "交叉编译:   ${CROSS_PREFIX}"
    [[ -n "$SYSROOT" ]] && log_info "Sysroot:    ${SYSROOT}"
fi
if [[ ${#COMPONENTS[@]} -eq 0 ]]; then
    log_info "编译组件:   全部"
else
    log_info "编译组件:   ${COMPONENTS[*]}"
fi
echo "========================================"

# 预编译库安装
should_build librga && install_librga
should_build rknn   && install_rknn

# 按依赖顺序编译
should_build mpp     && build_mpp
should_build x264    && build_x264
should_build x265    && build_x265
should_build libdrm  && build_libdrm
should_build m4      && build_m4
should_build autoconf && build_autoconf
should_build libtool && build_libtool
should_build bison   && build_bison
should_build flex    && build_flex
should_build openssl && build_openssl
should_build gsoap   && build_gsoap
should_build ffmpeg  && build_ffmpeg
should_build opencv  && build_opencv

echo "========================================"
log_ok "所有组件编译完成! 输出目录: ${INSTALL_PREFIX}"
