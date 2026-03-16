rm build -rf
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
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