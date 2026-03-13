#!/bin/bash

set -e # 遇到错误立即退出，防止后续操作基于不完整的代码

# 定义一个智能克隆函数
# 用法: smart_clone <git_url> [--branch <branch>] [<dir_name>]
smart_clone() {
    local GIT_URL=""
    local BRANCH=""
    local DIR_NAME=""
    local EXTRA_ARGS=()

    # 解析参数
    while [ $# -gt 0 ]; do
        case "$1" in
            --branch|-b)
                BRANCH="$2"
                EXTRA_ARGS+=(--branch "$2")
                shift 2
                ;;
            -*)
                EXTRA_ARGS+=("$1")
                shift
                ;;
            *)
                if [ -z "$GIT_URL" ]; then
                    GIT_URL="$1"
                else
                    DIR_NAME="$1"
                fi
                shift
                ;;
        esac
    done

    # 如果未指定目录名，从 URL 提取
    if [ -z "$DIR_NAME" ]; then
        DIR_NAME=$(basename "$GIT_URL" .git)
    fi

    if [ -d "$DIR_NAME" ]; then
        if [ -d "$DIR_NAME/.git" ]; then
            echo -e "\e[32m[SKIP]\e[0m $DIR_NAME already exists. Skipping clone."
            return 0
        else
            echo -e "\e[33m[WARN]\e[0m Directory '$DIR_NAME' exists but is not a git repo. Removing and re-cloning..."
            rm -rf "$DIR_NAME"
        fi
    fi

    echo -e "\e[34m[CLONE]\e[0m Cloning $DIR_NAME ..."
    git -c advice.detachedHead=false clone --depth 1 "${EXTRA_ARGS[@]}" "$GIT_URL" "$DIR_NAME"

    if [ $? -ne 0 ]; then
        echo -e "\e[31m[ERROR]\e[0m Failed to clone $DIR_NAME!"
        exit 1
    fi
}

# 定义一个智能下载函数 (wget + 解压)
# 参数: $1 = download_url, $2 = 解压后的目录名(可选), $3 = 保存文件名(可选)
smart_wget() {
    local URL=$1
    local DIR_NAME=$2
    local FILENAME
    if [ -n "$3" ]; then
        FILENAME=$3
    else
        FILENAME=$(basename "$URL")
    fi

    # 如果指定了目录名且目录已存在，跳过
    if [ -n "$DIR_NAME" ] && [ -d "$DIR_NAME" ]; then
        echo -e "\e[32m[SKIP]\e[0m $DIR_NAME already exists. Skipping download."
        return 0
    fi

    # 如果压缩包已存在但目录不存在，直接解压
    if [ -f "$FILENAME" ] && [ -n "$DIR_NAME" ]; then
        echo -e "\e[33m[INFO]\e[0m $FILENAME exists, extracting..."
    else
        echo -e "\e[34m[DOWNLOAD]\e[0m Downloading $FILENAME ..."
        wget -q --show-progress -O "$FILENAME" "$URL"
        if [ $? -ne 0 ]; then
            echo -e "\e[31m[ERROR]\e[0m Failed to download $FILENAME!"
            rm -f "$FILENAME"
            exit 1
        fi
    fi

    echo -e "\e[34m[EXTRACT]\e[0m Extracting $FILENAME ..."
    case "$FILENAME" in
        *.zip)
            unzip -qo "$FILENAME"
            ;;
        *)
            tar xf "$FILENAME"
            ;;
    esac
    if [ $? -ne 0 ]; then
        echo -e "\e[31m[ERROR]\e[0m Failed to extract $FILENAME!"
        exit 1
    fi
    rm -f "$FILENAME"
}

SRC_DIR="src"
mkdir -p "$SRC_DIR"
cd "$SRC_DIR"

echo "Starting repository synchronization..."
echo "----------------------------------------"

# rockchip sdk
smart_clone https://github.com/airockchip/rknn-toolkit2.git

# rockchip mpp
smart_clone https://github.com/rockchip-linux/mpp.git

# rockchip rga
smart_clone https://github.com/airockchip/librga.git

# x264
smart_clone https://code.videolan.org/videolan/x264.git

# x265
smart_wget https://download.videolan.org/videolan/x265/x265_4.1.tar.gz x265_4.1

# libdrm
smart_wget https://dri.freedesktop.org/libdrm/libdrm-2.4.123.tar.xz libdrm-2.4.123

# ffmpeg (主线版本)
# smart_clone https://git.ffmpeg.org/ffmpeg.git --branch n7.2-dev ffmpeg
# ffmpeg (rockchip 定制版, RGA滤镜+零拷贝优化)
smart_clone https://github.com/nyanmisaka/ffmpeg-rockchip.git --branch 8.1 ffmpeg

# opencv
smart_wget https://github.com/opencv/opencv/archive/refs/tags/4.13.0.tar.gz opencv-4.13.0

# bison
smart_wget https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.xz bison-3.8.2

# flex
smart_wget https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz flex-2.6.4

# gsoap
smart_wget https://sourceforge.net/projects/gsoap2/files/gsoap_2.8.134.zip/download gsoap-2.8 gsoap_2.8.134.zip

# libtool
smart_wget https://ftp.gnu.org/gnu/libtool/libtool-2.5.4.tar.xz libtool-2.5.4

# openssl
smart_wget https://github.com/openssl/openssl/releases/download/openssl-3.4.1/openssl-3.4.1.tar.gz openssl-3.4.1

# autoconf
smart_wget https://ftp.gnu.org/gnu/autoconf/autoconf-2.72.tar.xz autoconf-2.72

# m4
smart_wget https://ftp.gnu.org/gnu/m4/m4-1.4.19.tar.xz m4-1.4.19

# rockchip media
# smart_clone <rockchip_media_url_here>

echo "----------------------------------------"
echo -e "\e[32mAll repositories synchronized successfully!\e[0m"