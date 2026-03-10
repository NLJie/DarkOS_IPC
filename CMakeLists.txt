# Top-level CMake entry for the project.

cmake_minimum_required(VERSION 3.16)

project(pg_program C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置平台相关的编译选项
option(PG_HAL_USE_X86 "Use X86 HAL port implementation" ON)
option(PG_HAL_USE_RK3588S "Use RK3588S HAL port implementation" OFF)

if(PG_HAL_USE_X86)
	add_subdirectory(platform/x86)
endif()

if(PG_HAL_USE_RK3588S)
	add_subdirectory(platform/rk3588s)
endif()

add_subdirectory(components/utils)
add_subdirectory(components/hal)
add_subdirectory(third_party/cJSON)
add_subdirectory(applications/default_app)

