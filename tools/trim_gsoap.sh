#!/bin/bash
# =============================================================================
# trim_gsoap.sh — 从 gsoap 压缩包或源码目录提取 ONVIF 客户端所需的最小文件集
#
# 用法:
#   ./tools/trim_gsoap.sh <gsoap压缩包或源码目录>
#
# 示例:
#   ./tools/trim_gsoap.sh third_party/gsoap_2.8.140.zip
#   ./tools/trim_gsoap.sh /path/to/gsoap-2.8
#
# 运行完成后:
#   third_party/gsoap/  — 精简库文件，可直接提交到工程
#   tools/bin/          — wsdl2h / soapcpp2 宿主工具
# 压缩包解压的临时目录会自动删除，原始压缩包保留。
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DST="$PROJECT_ROOT/third_party/gsoap"

if [ -z "$1" ]; then
    echo "用法: $0 <gsoap压缩包(.zip/.tar.gz)或源码目录>"
    exit 1
fi

INPUT="$(realpath "$1")"
TEMP_DIR=""

# ------------------------------------------------------------------
# 0. 如果输入是压缩包，解压到临时目录
# ------------------------------------------------------------------
if [ -f "$INPUT" ]; then
    TEMP_DIR=$(mktemp -d)
    echo "[0/5] 解压 $(basename "$INPUT") 到临时目录..."
    case "$INPUT" in
        *.zip)     unzip -q "$INPUT" -d "$TEMP_DIR" ;;
        *.tar.gz)  tar -xzf "$INPUT" -C "$TEMP_DIR" ;;
        *.tar.bz2) tar -xjf "$INPUT" -C "$TEMP_DIR" ;;
        *) echo "错误: 不支持的压缩格式，请使用 .zip / .tar.gz / .tar.bz2"; rm -rf "$TEMP_DIR"; exit 1 ;;
    esac
    # 找到同时包含 stdsoap2.h + stdsoap2.cpp + plugin/ 的目录（排除文档目录）
    GSOAP_SRC=""
    while IFS= read -r candidate; do
        dir=$(dirname "$candidate")
        if [ -f "$dir/stdsoap2.cpp" ] && [ -d "$dir/plugin" ]; then
            GSOAP_SRC="$dir"
            break
        fi
    done < <(find "$TEMP_DIR" -name "stdsoap2.h" -not -path "*/VisualStudio*" -not -path "*/doc/*")
    if [ -z "$GSOAP_SRC" ]; then
        echo "错误: 压缩包内未找到有效的 gsoap 源码目录（需含 stdsoap2.h + stdsoap2.cpp + plugin/）"
        rm -rf "$TEMP_DIR"
        exit 1
    fi
elif [ -d "$INPUT" ]; then
    if [ -f "$INPUT/stdsoap2.h" ]; then
        GSOAP_SRC="$INPUT"
    elif [ -f "$INPUT/gsoap/stdsoap2.h" ]; then
        GSOAP_SRC="$INPUT/gsoap"
    else
        echo "错误: 在 $INPUT 下找不到 stdsoap2.h"
        exit 1
    fi
else
    echo "错误: $INPUT 不是文件或目录"
    exit 1
fi

echo "源目录: $GSOAP_SRC"
echo "目标目录: $DST"
echo ""

# 清理旧版本
rm -rf "$DST"
mkdir -p "$DST/gsoap" "$DST/plugin" "$DST/import"

# ------------------------------------------------------------------
# 1. 核心运行时
# ------------------------------------------------------------------
echo "[1/5] 复制核心运行时..."
cp "$GSOAP_SRC/stdsoap2.h"   "$DST/gsoap/"
cp "$GSOAP_SRC/stdsoap2.cpp" "$DST/gsoap/"
cp "$GSOAP_SRC/dom.cpp"      "$DST/gsoap/"

# ------------------------------------------------------------------
# 2. 插件: WS-Security (lite, 无需 OpenSSL) + WS-Discovery
# ------------------------------------------------------------------
echo "[2/5] 复制插件..."
cp "$GSOAP_SRC/plugin/wsseapi-lite.h" "$DST/plugin/"
cp "$GSOAP_SRC/plugin/wsseapi-lite.c" "$DST/plugin/"
cp "$GSOAP_SRC/plugin/wsddapi.h"      "$DST/plugin/"
cp "$GSOAP_SRC/plugin/wsddapi.c"      "$DST/plugin/"

# ------------------------------------------------------------------
# 3. Import 头文件 + typemap.dat
# ------------------------------------------------------------------
echo "[3/5] 复制 import 头文件..."
IMPORT_FILES=(
    "wsse.h"      # WS-Security schema
    "wsa5.h"      # WS-Addressing (2005)
    "wsdd10.h"    # WS-Discovery 1.0
    "wsdd.h"      # WS-Discovery (older, 部分 ONVIF 设备用)
    "ds.h"        # XML Digital Signature
    "ref.h"       # 内部引用 schema
    "soap12.h"    # SOAP 1.2 命名空间绑定
    "xop.h"       # XML-binary Optimized Packaging
    "typemap.dat" # wsdl2h 命名空间映射
)
for f in "${IMPORT_FILES[@]}"; do
    if [ -f "$GSOAP_SRC/import/$f" ]; then
        cp "$GSOAP_SRC/import/$f" "$DST/import/"
    else
        echo "  警告: import/$f 不存在，跳过"
    fi
done

# ------------------------------------------------------------------
# 4. 复制宿主工具 (wsdl2h / soapcpp2) 并 strip 调试符号
# ------------------------------------------------------------------
echo "[4/5] 复制宿主工具..."
TOOLS_BIN="$PROJECT_ROOT/tools/bin"
mkdir -p "$TOOLS_BIN"
for tool in soapcpp2 wsdl2h; do
    found=$(find "$GSOAP_SRC" -maxdepth 2 -name "$tool" -type f | grep -v VisualStudio | head -1)
    if [ -n "$found" ]; then
        cp "$found" "$TOOLS_BIN/$tool"
        strip "$TOOLS_BIN/$tool" 2>/dev/null || true
        chmod +x "$TOOLS_BIN/$tool"
        SIZE=$(du -sh "$TOOLS_BIN/$tool" | cut -f1)
        echo "  $tool → tools/bin/$tool ($SIZE)"
    else
        echo "  警告: $tool 未找到（压缩包内可能未包含预编译二进制）"
    fi
done

# ------------------------------------------------------------------
# 5. 生成 CMakeLists.txt
# ------------------------------------------------------------------
cat > "$DST/CMakeLists.txt" << 'CMAKEOF'
cmake_minimum_required(VERSION 3.16)
project(gsoap VERSION 2.8 LANGUAGES C CXX)

# gsoap 核心运行时 (插件在 onvif 组件里与桩代码一起编译)
add_library(gsoap STATIC
    gsoap/stdsoap2.cpp
    gsoap/dom.cpp
)

target_include_directories(gsoap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/gsoap
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/import
)

if(UNIX)
    target_link_libraries(gsoap PUBLIC pthread)
endif()
CMAKEOF

# ------------------------------------------------------------------
# 6. 清理临时解压目录
# ------------------------------------------------------------------
if [ -n "$TEMP_DIR" ]; then
    echo "[5/5] 清理临时目录..."
    rm -rf "$TEMP_DIR"
else
    echo "[5/5] 跳过清理（输入为目录）"
fi

echo ""
echo "完成！提取文件列表:"
find "$DST" -type f | sort | sed "s|$DST/||"
echo ""
echo "精简后文件数: $(find "$DST" -type f | wc -l)"
