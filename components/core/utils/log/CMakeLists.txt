add_library(log_system STATIC)

target_sources(log_system
    PRIVATE
        src/log_system.cpp
)

target_include_directories(log_system
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
