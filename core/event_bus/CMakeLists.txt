cmake_minimum_required(VERSION 3.16)

add_library(pg_event_bus STATIC
    src/pg_event_bus.c
)

target_include_directories(pg_event_bus
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# POSIX: pthread + fnmatch
target_link_libraries(pg_event_bus
    PUBLIC pthread
)

target_compile_definitions(pg_event_bus
    PRIVATE _POSIX_C_SOURCE=200809L
)
