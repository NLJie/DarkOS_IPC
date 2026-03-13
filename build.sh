rm build -rf
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
# cd products/cam_basic
# ./cam_basic --config ../../../platform/configs/device_x86_usb_cam_v1.json

#  cmake .. -DPG_PLATFORM=rk3576 \
        #    -DCMAKE_TOOLCHAIN_FILE=../platform/rk3576/toolchain.cmake