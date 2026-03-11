add_executable(default_app
    main.cpp
    app_runtime_config.cpp
    src/app_camera.cpp
    src/app_mem.cpp
)

target_include_directories(default_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(default_app
    PRIVATE
        common
        hal_camera
        log_system
)
