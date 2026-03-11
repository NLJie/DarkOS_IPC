#!/usr/bin/env bash
# ...existing code...

set -euo pipefail

# 项目根目录（脚本位于 project_root/temp）
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BSP_DIR="${ROOT_DIR}/platform/x86/bsp"
JOBS="${JOBS:-$(nproc)}"

# 构建日志目录（默认写到 temp/.build_logs）
LOG_DIR="${LOG_DIR:-${WORK_DIR}/.build_logs}"
mkdir -p "${LOG_DIR}"

# 统一安装目录
BISON_PREFIX="${BSP_DIR}/bison"
FLEX_PREFIX="${BSP_DIR}/flex"
OPENSSL_PREFIX="${BSP_DIR}/openssl"
GSOAP_PREFIX="${BSP_DIR}/gsoap"

log() { echo "[onvif-build] $*"; }
die() { echo "[onvif-build][ERROR] $*" >&2; exit 1; }

run_quiet() {
    local step="$1"
    local logfile="$2"
    shift 2

    log ">> ${step}"
    if ! "$@" >"${logfile}" 2>&1; then
        echo "[onvif-build][ERROR] ${step} 失败，日志: ${logfile}" >&2
        echo "------ 日志末尾（80行）------" >&2
        tail -n 80 "${logfile}" >&2 || true
        exit 1
    fi
}

require_file() {
    local f="$1"
    [[ -f "${WORK_DIR}/${f}" ]] || die "缺少源码包: ${WORK_DIR}/${f}"
}

extract_tar_gz() {
    local archive="$1"
    local src_dir="${2:-}"
    require_file "${archive}"

    # 避免旧目录残留导致混合构建
    if [[ -n "${src_dir}" ]]; then
        rm -rf "${WORK_DIR}/${src_dir}"
    fi

    run_quiet "解压 ${archive}" "${LOG_DIR}/${archive}.extract.log" \
        tar -xzf "${WORK_DIR}/${archive}"
}

extract_zip() {
    local archive="$1"
    local src_dir="${2:-}"
    require_file "${archive}"

    # 避免 unzip 进入交互覆盖提示
    if [[ -n "${src_dir}" ]]; then
        rm -rf "${WORK_DIR}/${src_dir}"
    fi

    run_quiet "解压 ${archive}" "${LOG_DIR}/${archive}.extract.log" \
        unzip -oq "${WORK_DIR}/${archive}"
}

clean_prefix() {
    local prefix="$1"
    rm -rf "${prefix}"
    mkdir -p "${prefix}"
}

build_autotools_pkg() {
    local src_dir="$1"
    local prefix="$2"
    shift 2
    clean_prefix "${prefix}"

    pushd "${src_dir}" >/dev/null
    run_quiet "${src_dir}: configure" "${LOG_DIR}/${src_dir}.configure.log" \
        ./configure --prefix="${prefix}" "$@"
    run_quiet "${src_dir}: make" "${LOG_DIR}/${src_dir}.make.log" \
        make -j"${JOBS}"
    run_quiet "${src_dir}: install" "${LOG_DIR}/${src_dir}.install.log" \
        make install
    popd >/dev/null

    # rm -rf "${prefix}/share"
    rm -rf "${src_dir}"
}

build_openssl() {
    local src_dir="$1"
    local prefix="$2"
    clean_prefix "${prefix}"

    pushd "${src_dir}" >/dev/null
    run_quiet "${src_dir}: config" "${LOG_DIR}/${src_dir}.config.log" \
        ./config --prefix="${prefix}"
    run_quiet "${src_dir}: make" "${LOG_DIR}/${src_dir}.make.log" \
        make -j"${JOBS}"
    run_quiet "${src_dir}: install" "${LOG_DIR}/${src_dir}.install.log" \
        make install
    popd >/dev/null

    rm -rf "${src_dir}"
}

main() {
    mkdir -p "${BSP_DIR}"
    cd "${WORK_DIR}"

    log "[1/4] 构建 bison..."
    extract_tar_gz "bison-3.8.tar.gz"
    build_autotools_pkg "bison-3.8" "${BISON_PREFIX}"

    log "[2/4] 构建 flex..."
    extract_tar_gz "flex-2.6.4.tar.gz"
    build_autotools_pkg "flex-2.6.4" "${FLEX_PREFIX}"

    log "[3/4] 构建 openssl..."
    extract_tar_gz "openssl-1.1.1v.tar.gz"
    build_openssl "openssl-1.1.1v" "${OPENSSL_PREFIX}"

    log "[4/4] 构建 gsoap..."
    extract_zip "gsoap_2.8.131.zip"

    # 确保 gsoap configure 能优先找到本地工具链
    export PATH="${FLEX_PREFIX}/bin:${BISON_PREFIX}/bin:${PATH}"

    build_autotools_pkg "gsoap-2.8" "${GSOAP_PREFIX}" \
        --with-openssl="${OPENSSL_PREFIX}" \
        --with-flex="${FLEX_PREFIX}" \
        --with-bison="${BISON_PREFIX}"

    log "全部完成。输出目录: ${BSP_DIR}"
    log "详细日志目录: ${LOG_DIR}"
}

main "$@"

# export LD_LIBRARY_PATH=/home/nlj/workspace1/P000_rk3576_zs/project_root/platform/x86/bsp/openssl/lib:$LD_LIBRARY_PATH

# mkdir onvif && cd onvif
# SOAP2: http://www.w3.org/2003/05/soap-envelope
# ../wsdl2h -o ptz.h -c -s -t typemap.dat https://www.onvif.org/ver20/ptz/wsdl/ptz.wsdl
# 其中-c为产生纯c代码，默认生成 c++代码；-s为不使用STL库，-t为typemap.dat的标识

# ../soapcpp2  -2 -C -c -x -Iimport ptz.h 
# -2为生成SOAP 1.2版本的代码，-C为仅生成客户端的代码（服务端的不要），-c生成C语言代码，-Iimport导入头文件