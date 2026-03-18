# rm build -rf
# mkdir build
# cd build
# cmake .. -DCMAKE_BUILD_TYPE=Release
# make -j$(nproc)

# cd products/cam_basic
# ./cam_basic --config ../../../platform/configs/device_x86_usb_cam_v1.json

#  cmake .. -DPG_PLATFORM=rk3576 \
        #    -DCMAKE_TOOLCHAIN_FILE=../platform/rk3576/toolchain.cmake

# Host 主机上需要安装的工具

# sudo apt install -y \
#         cmake make ninja-build meson pkg-config \
#         gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu \
#         m4 autoconf libtool autotools-dev \
#         bison flex \
#         nasm \
#         libsdl2-dev \
#         perl


# ./compile.sh --clean ffmpeg
# ./compile.sh --clean opencv


  # 1. 编译 MPP 库（通过 compile.sh）
#   cd tools
#   ./compile.sh --arch=aarch64 --platform=rk3588s \
#       --cross=/home/nlj/workspace1/P000_rk3576_zs/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- \
#       mpp libdrm

  # 2. 编译主项目
  rm build
  mkdir -p build && cd build
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/rk3588s_toolchain.cmake \
           -DPG_PRODUCT=cam_basic
  make -j$(nproc)

  # 3. 部署到 RK3588S
#   scp products/cam_basic/cam_basic root@192.168.1.100:/userdata/
#   scp ../platform/configs/device_rk3588s_ov8858.json root@192.168.1.100:/userdata/