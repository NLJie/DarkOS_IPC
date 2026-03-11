# Top-level CMake entry for the project.

cmake_minimum_required(VERSION 3.16)

project(pg_program C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 平台
set(PG_PLATFORM "x86" CACHE STRING "Target: x86 | rk3576 | rk3588s | rv1126b")

# 产品（功能集合）
set(PG_PRODUCT "cam_basic" CACHE STRING "Product: cam_basic | cam_ai | cam_streaming")

# 加载平台
add_subdirectory(platform/${PG_PLATFORM})

# 第三方库
add_subdirectory(third_party/cJSON)

# 加载框架核心
add_subdirectory(core/hal)
add_subdirectory(core/utils)       # 含 log / common / json
add_subdirectory(core/event_bus)
add_subdirectory(core/device_config)

# 加载产品声明 (设置 PG_ENABLE_xxx 变量)
include(products/${PG_PRODUCT}/product.cmake)

# 按产品声明条件加载 components
if(PG_ENABLE_MEDIA_CORE)
    add_subdirectory(components/media_core)
endif()
if(PG_ENABLE_AI_ENGINE)
    add_subdirectory(components/ai_engine)
endif()
if(PG_ENABLE_ALARM_SYSTEM)
    add_subdirectory(components/alarm_system)
endif()
if(PG_ENABLE_NETWORK_STACK)
    add_subdirectory(components/network_stack)
endif()
if(PG_ENABLE_OTA)
    add_subdirectory(components/ota)
endif()
if(PG_ENABLE_STORAGE)
    add_subdirectory(components/storage_service)
endif()

# 编译产品可执行文件
add_subdirectory(products/${PG_PRODUCT})

