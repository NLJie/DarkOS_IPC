add_executable(default_app
    main.cpp
    app_runtime_config.cpp
)

target_link_libraries(default_app
    PRIVATE
        common
        hal_camera
        log_system
)
