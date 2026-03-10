cmake_minimum_required(VERSION 3.5)
project(cJSON VERSION 1.7.19 LANGUAGES C)

# 1. 核心选项设置 (默认关闭不需要的功能)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF) # 推荐默认 OFF (静态库)
option(ENABLE_CJSON_TEST "Enable building cJSON test" OFF) # 强制关闭测试
option(ENABLE_CJSON_UTILS "Enable building the cJSON_Utils library" OFF) # 除非你用到 cJSON_Utils，否则关闭
option(ENABLE_LOCALES "Enable the use of locales" OFF) # 嵌入式通常不需要本地化
option(ENABLE_CUSTOM_COMPILER_FLAGS "Enables custom compiler flags" OFF) # 【关键】关闭严格检查，防止老旧编译器报错

# 2. 定义库源文件
set(CJSON_LIB cjson)
file(GLOB HEADERS cJSON.h)
set(SOURCES cJSON.c)

# 3. 创建库 (只创建库，不做其他花哨操作)
add_library(${CJSON_LIB} STATIC ${HEADERS} ${SOURCES}) # 强制 STATIC，或者用 ${BUILD_SHARED_LIBS}

# 设置包含目录，方便主项目 include "cJSON.h"
target_include_directories(${CJSON_LIB} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 4. 链接数学库 (Linux/Unix 需要)
if(NOT WIN32 AND NOT ANDROID)
    target_link_libraries(${CJSON_LIB} m)
endif()


# 如果开启了 Utils (可选)
if(ENABLE_CJSON_UTILS)
    set(CJSON_UTILS_LIB cjson_utils)
    file(GLOB HEADERS_UTILS cJSON_Utils.h)
    set(SOURCES_UTILS cJSON_Utils.c)
    
    add_library(${CJSON_UTILS_LIB} STATIC ${HEADERS_UTILS} ${SOURCES_UTILS})
    target_include_directories(${CJSON_UTILS_LIB} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${CJSON_UTILS_LIB} ${CJSON_LIB})
    if(NOT WIN32 AND NOT ANDROID)
        target_link_libraries(${CJSON_UTILS_LIB} m)
    endif()
endif()