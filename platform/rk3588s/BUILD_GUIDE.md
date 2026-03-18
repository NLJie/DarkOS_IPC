# RK3588S 平台编译指南

## 概述

本文档说明如何为 RK3588S 平台（带 OV8858 MIPI 摄像头）交叉编译 PG Camera 项目。

## 硬件要求

- **开发板**: RK3588S 开发板
- **摄像头**: OV8858 MIPI CSI 摄像头 (或其他 MIPI 摄像头)
- **网络**: 开发板需要联网或通过 SCP 传输文件

## 软件要求

### 1. 交叉编译工具链

工具链路径：
```
/home/nlj/workspace1/P000_rk3576_zs/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu
```

如需修改，编辑 `cmake/rk3588s_toolchain.cmake` 或使用环境变量：
```bash
export RK3588S_TOOLCHAIN_PATH=/your/toolchain/path
```

## 编译步骤

### 1. 编译 MPP 库（通过 compile.sh）

```bash
cd tools

# 编译 MPP 库到 platform/rk3588s/bsp/
./compile.sh --arch=aarch64 --platform=rk3588s \
    --cross=/home/nlj/workspace1/P000_rk3576_zs/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- \
    mpp

# 如需同时编译其他依赖库（推荐）
./compile.sh --arch=aarch64 --platform=rk3588s \
    --cross=/home/nlj/workspace1/P000_rk3576_zs/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- \
    mpp libdrm
```

编译完成后，库文件会输出到 `platform/rk3588s/bsp/`：
```
platform/rk3588s/bsp/
├── include/
│   └── rk_media/
│       ├── rk_mpi_vi.h
│       ├── rk_mpi_sys.h
│       └── ...
└── lib/
    ├── librockchip_mpp.so
    ├── librk_mpi_vi.so
    └── ...
```

### 2. 配置主项目编译

```bash
mkdir -p build_rk3588s
cd build_rk3588s

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/rk3588s_toolchain.cmake \
    -DPG_PRODUCT=cam_basic \
    -DCMAKE_BUILD_TYPE=Release
```

### 3. 编译

```bash
make -j$(nproc)
```

### 4. 检查输出

```bash
# 确认生成的是 ARM64 可执行文件
file products/cam_basic/cam_basic
# 输出应包含: "ARM aarch64"
```

## 部署到 RK3588S

### 1. 拷贝可执行文件

```bash
# 在开发主机上
scp build_rk3588s/products/cam_basic/cam_basic root@192.168.1.100:/userdata/
scp platform/configs/device_rk3588s_ov8858.json root@192.168.1.100:/userdata/
```

### 2. 在 RK3588S 上运行

```bash
# 在 RK3588S 板子上
ssh root@192.168.1.100
cd /userdata
chmod +x cam_basic

# 运行
./cam_basic --config device_rk3588s_ov8858.json
```

## OV8858 摄像头配置

配置文件: `platform/configs/device_rk3588s_ov8858.json`

### 常用分辨率

| 分辨率 | 说明 |
|--------|------|
| 3264x2448 | OV8858 最大分辨率 (4:3) |
| 1920x1080 | 1080P 全高清 (推荐) |
| 1280x720  | 720P 高清 |
| 640x480   | VGA |

修改配置：
```json
{
  "cameras": [
    {
      "interface": "mipi",
      "sensor": "ov8858",
      "width": 1920,
      "height": 1080,
      "fps": 30.0
    }
  ]
}
```

### MIPI 接口配置

RK3588S 的 MIPI CSI 接口：
- **MIPI0**: 通常连接主摄像头
- **MIPI1**: 通常连接副摄像头

如需修改设备号，编辑 HAL 代码中的 `vi_dev` 值。

## 常见问题

### 1. 找不到 MPP 库

**错误**: `RK3588S MPP libraries not found in .../platform/rk3588s/bsp/lib`

**解决**: 
```bash
# 先运行 compile.sh 编译 MPP
cd tools
./compile.sh --arch=aarch64 --platform=rk3588s --cross=... mpp
```

### 2. 运行时错误 "RK_MPI_SYS_Init failed"

**原因**: MPP 系统已经在运行，或驱动未加载

**解决**:
```bash
# 在 RK3588S 上检查
dmesg | grep -i mpp
ls /dev/rga /dev/mpp_service

# 重启 MPP
killall cam_basic
rmmod mpp_service  # 如果需要
insmod /ko/mpp_service.ko
```

### 3. 摄像头无图像

**排查步骤**:
```bash
# 1. 检查摄像头是否被识别
ls /dev/video*
dmesg | grep -i ov8858

# 2. 检查设备树配置
cat /proc/device-tree/mipi/

# 3. 使用 v4l2 测试
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/video0 --stream-mmap
```

### 4. 交叉编译器版本不匹配

**解决**: 确保使用与目标板内核匹配的编译器版本。RK3588S 通常使用 GCC 10.x 或 12.x。

## 调试技巧

### 开启 MPP 调试日志

```bash
export mpp_debug=1
export mpp_log_level=7
./cam_basic --config device_rk3588s_ov8858.json
```

### 使用 GDB 远程调试

```bash
# 在 RK3588S 上
gdbserver :1234 ./cam_basic --config device_rk3588s_ov8858.json

# 在开发主机上
/home/nlj/workspace1/P000_rk3576_zs/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gdb \
    ./build_rk3588s/products/cam_basic/cam_basic
target remote 192.168.1.100:1234
```

## 完整编译流程（一键脚本）

```bash
#!/bin/bash
# build_rk3588s.sh - 完整的 RK3588S 编译流程

set -e

TOOLCHAIN="/home/nlj/workspace1/P000_rk3576_zs/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-"

echo "=== Step 1: Build MPP library ==="
cd tools
./compile.sh --arch=aarch64 --platform=rk3588s --cross="${TOOLCHAIN}" mpp libdrm
cd ..

echo "=== Step 2: Configure project ==="
mkdir -p build_rk3588s
cd build_rk3588s
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/rk3588s_toolchain.cmake \
    -DPG_PRODUCT=cam_basic \
    -DCMAKE_BUILD_TYPE=Release

echo "=== Step 3: Build project ==="
make -j$(nproc)

echo "=== Done ==="
echo "Output: build_rk3588s/products/cam_basic/cam_basic"
file products/cam_basic/cam_basic
```

## 参考文档

- [Rockchip MPP 文档](https://github.com/rockchip-linux/mpp/tree/develop/doc)
- [RK3588 TRM (Technical Reference Manual)](https://www.rock-chips.com/uploads/pdf/2022.8.26/191/RK3588%20TRM%20Part1%20V1.0-20220309.pdf)
- [OV8858 数据手册](https://www.ovt.com/download/sensor_pdf/OV8858.pdf)
