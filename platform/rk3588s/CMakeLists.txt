# RK3588S Platform CMake Configuration
# 使用 Rockit SDK (包含 MPI 接口)

cmake_minimum_required(VERSION 3.16)

# 设置 RK3588S 平台标识
set(PG_HAL_USE_RK3588S ON CACHE BOOL "" FORCE)

# BSP 路径
set(RK3588S_BSP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/bsp")
set(RK3588S_BSP_INCLUDE "${RK3588S_BSP_ROOT}/include")
set(RK3588S_BSP_LIB "${RK3588S_BSP_ROOT}/lib")

# ==================== 头文件配置 ====================
# 使用 Rockit SDK 的 rk_media 头文件

set(RK3588S_INCLUDE_DIRS
    ${RK3588S_BSP_INCLUDE}
    ${RK3588S_BSP_INCLUDE}/rk_media
)

# 检查关键头文件
if(NOT EXISTS "${RK3588S_BSP_INCLUDE}/rk_media/rk_mpi_vi.h")
    message(FATAL_ERROR "RK3588S SDK headers not found in ${RK3588S_BSP_INCLUDE}/rk_media\n"
        "Please ensure SDK headers are in platform/rk3588s/bsp/include/rk_media/")
endif()

message(STATUS "RK3588S SDK headers: ${RK3588S_BSP_INCLUDE}/rk_media")

# 创建平台头文件接口库
add_library(pg_platform_rk3588s_headers INTERFACE)

target_include_directories(pg_platform_rk3588s_headers
    INTERFACE
        ${RK3588S_INCLUDE_DIRS}
)

# ==================== 库文件配置 ====================

# 查找 Rockit 库（优先使用 SDK 的整合库）
find_library(RK_ROCKIT_LIB rockit
    PATHS ${RK3588S_BSP_LIB}
    NO_DEFAULT_PATH
)

# 也查找开源 MPP 库（编解码可能用到）
find_library(RK_MPP_LIB rockchip_mpp
    PATHS ${RK3588S_BSP_LIB}
    NO_DEFAULT_PATH
)

set(RK3588S_MPP_LIBS_FOUND)

if(RK_ROCKIT_LIB)
    list(APPEND RK3588S_MPP_LIBS_FOUND ${RK_ROCKIT_LIB})
    message(STATUS "Found rockit: ${RK_ROCKIT_LIB}")
else()
    # 如果没有 rockit，尝试找分开的 SDK 库
    set(RK_SDK_LIBS
        rk_mpi_vi
        rk_mpi_sys
        rk_mpi_mb
        rk_mpi_venc
        rk_mpi_vdec
    )
    foreach(lib ${RK_SDK_LIBS})
        find_library(RK_LIB_${lib} ${lib}
            PATHS ${RK3588S_BSP_LIB}
            NO_DEFAULT_PATH
        )
        if(RK_LIB_${lib})
            list(APPEND RK3588S_MPP_LIBS_FOUND ${RK_LIB_${lib}})
            message(STATUS "Found ${lib}: ${RK_LIB_${lib}}")
        endif()
    endforeach()
endif()

if(RK_MPP_LIB)
    list(APPEND RK3588S_MPP_LIBS_FOUND ${RK_MPP_LIB})
    message(STATUS "Found rockchip_mpp: ${RK_MPP_LIB}")
endif()

# ==================== MPP 接口库 ====================

add_library(pg_mpp_rk3588s INTERFACE)

if(RK3588S_MPP_LIBS_FOUND)
    target_link_libraries(pg_mpp_rk3588s INTERFACE ${RK3588S_MPP_LIBS_FOUND})
    message(STATUS "RK3588S libraries linked from BSP")
else()
    message(FATAL_ERROR "No RK3588S libraries found in ${RK3588S_BSP_LIB}\n"
        "Please copy Rockit SDK libraries from:\n"
        "  /home/nlj/workspace1/P000_rk3576_zs/lubancat/external/rockit/mpi/sdk/lib/lib64/\n"
        "or from RK3588S board:\n"
        "  scp root@rk3588s:/usr/lib/librockit.so* platform/rk3588s/bsp/lib/")
endif()

target_include_directories(pg_mpp_rk3588s
    INTERFACE
        ${RK3588S_INCLUDE_DIRS}
)

# 导出变量
set(RK3588S_MPP_LIBS ${RK3588S_MPP_LIBS_FOUND} PARENT_SCOPE)
set(RK3588S_BSP_ROOT ${RK3588S_BSP_ROOT} PARENT_SCOPE)
