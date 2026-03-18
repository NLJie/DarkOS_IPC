cmake_minimum_required(VERSION 3.16)

add_executable(cam_basic
    main.cpp
    src/app_camera.cpp
    src/app_mem.cpp
    src/video_recorder.cpp
)

target_include_directories(cam_basic
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cam_basic
    PRIVATE
        pg_device_config   # 含 pg_config / hal_camera / log_system
        common
        pg_event_bus
)

# OpenCV 和 FFmpeg 是可选依赖
if(TARGET pg_opencv)
    target_link_libraries(cam_basic PRIVATE pg_opencv)
    target_compile_definitions(cam_basic PRIVATE PG_HAS_OPENCV=1)
endif()

if(TARGET pg_ffmpeg)
    target_link_libraries(cam_basic PRIVATE pg_ffmpeg)
    target_compile_definitions(cam_basic PRIVATE PG_HAS_FFMPEG=1)
endif()

# RK3588S: 依赖的 DRM/ALSA 等库在板子上已存在，交叉编译时允许未定义符号
if(PG_HAL_USE_RK3588S)
    target_link_options(cam_basic PRIVATE -Wl,--allow-shlib-undefined)
endif()
