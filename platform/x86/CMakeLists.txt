cmake_minimum_required(VERSION 3.16)

# 告知 hal_camera 使用 x86 后端
set(PG_HAL_USE_X86 ON CACHE BOOL "" FORCE)

set(X86_BSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bsp")

# 将 bsp 目录加入 CMake 搜索路径，后续 find_package / find_library 均在此查找
list(PREPEND CMAKE_PREFIX_PATH "${X86_BSP_DIR}")
list(PREPEND CMAKE_PREFIX_PATH "${X86_BSP_DIR}/lib/cmake")

# pkg-config 也指向 bsp
set(ENV{PKG_CONFIG_PATH} "${X86_BSP_DIR}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

# =============================================================================
# FFmpeg
# =============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)
pkg_check_modules(FFMPEG_EXTRA IMPORTED_TARGET
    libavdevice
    libavfilter
    libswresample
)

# pg_ffmpeg: 聚合 target，外部只需 target_link_libraries(xxx pg_ffmpeg)
add_library(pg_ffmpeg INTERFACE)
target_link_libraries(pg_ffmpeg INTERFACE
    PkgConfig::FFMPEG
    PkgConfig::FFMPEG_EXTRA
)

# =============================================================================
# OpenCV
# =============================================================================
set(OpenCV_DIR "${X86_BSP_DIR}/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)

# pg_opencv: 聚合 target
add_library(pg_opencv INTERFACE)
target_link_libraries(pg_opencv INTERFACE ${OpenCV_LIBS})
target_include_directories(pg_opencv INTERFACE ${OpenCV_INCLUDE_DIRS})
