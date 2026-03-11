cmake_minimum_required(VERSION 3.16)

add_library(pg_json STATIC
    src/pg_json.c
)

target_include_directories(pg_json
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pg_json
    PUBLIC cjson
)
