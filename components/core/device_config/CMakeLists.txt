cmake_minimum_required(VERSION 3.16)

add_library(pg_device_config STATIC
    src/pg_device_config.c
)

target_include_directories(pg_device_config
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pg_device_config
    PUBLIC
        pg_json      # 通用 JSON 读取
        hal_camera   # HalCamConfig
        log_system   # LogConfig / LogLevel
)
