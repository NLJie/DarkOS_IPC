# RK3588S 交叉编译工具链配置
# 使用方法: cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/rk3588s_toolchain.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 工具链路径（可自定义）
if(NOT DEFINED ENV{RK3588S_TOOLCHAIN_PATH})
    set(RK3588S_TOOLCHAIN_PATH "/home/nlj/workspace1/P000_rk3576_zs/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu")
else()
    set(RK3588S_TOOLCHAIN_PATH $ENV{RK3588S_TOOLCHAIN_PATH})
endif()

# 指定交叉编译器
set(CMAKE_C_COMPILER   ${RK3588S_TOOLCHAIN_PATH}/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${RK3588S_TOOLCHAIN_PATH}/bin/aarch64-none-linux-gnu-g++)
set(CMAKE_AR           ${RK3588S_TOOLCHAIN_PATH}/bin/aarch64-none-linux-gnu-ar)
set(CMAKE_RANLIB       ${RK3588S_TOOLCHAIN_PATH}/bin/aarch64-none-linux-gnu-ranlib)
set(CMAKE_STRIP        ${RK3588S_TOOLCHAIN_PATH}/bin/aarch64-none-linux-gnu-strip)

# 查找路径限制在目标系统
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# RK3588S 平台标识
set(PG_PLATFORM "rk3588s" CACHE STRING "Target platform" FORCE)
set(PG_HAL_USE_RK3588S ON CACHE BOOL "Use RK3588S HAL" FORCE)

# MPP 库搜索路径（可从环境变量或默认位置获取）
if(NOT DEFINED ENV{RK3588S_MPP_PATH})
    # 默认使用项目内的 third_party/mpp
    set(RK3588S_MPP_PATH ${CMAKE_CURRENT_LIST_DIR}/../third_party/mpp/rk3588s)
else()
    set(RK3588S_MPP_PATH $ENV{RK3588S_MPP_PATH})
endif()

message(STATUS "RK3588S MPP Path: ${RK3588S_MPP_PATH}")
