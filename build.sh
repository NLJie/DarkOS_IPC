rm build -rf
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)


#  cmake .. -DPG_PLATFORM=rk3576 \
        #    -DCMAKE_TOOLCHAIN_FILE=../platform/rk3576/toolchain.cmake