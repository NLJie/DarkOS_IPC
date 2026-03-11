cmake_minimum_required(VERSION 3.16)

add_executable(cam_basic
    main.cpp
    app_runtime_config.cpp
    src/app_camera.cpp
    src/app_mem.cpp
)

target_include_directories(cam_basic
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cam_basic
    PRIVATE
        hal_camera
        log_system
        common
        pg_event_bus
        cjson
)
