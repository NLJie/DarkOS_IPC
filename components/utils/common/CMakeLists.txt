add_library(common STATIC)

target_sources(common
    PRIVATE
        src/com_mem_monitor.cpp
)

target_include_directories(common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
