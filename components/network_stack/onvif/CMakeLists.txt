add_library(pg_onvif STATIC
    src/pg_onvif.c
)

# 关键：插件 .c 文件按 C++ 编译（其头链路包含 <vector>）
set_source_files_properties(${ONVIF_GSOAP_PLUGIN_SRCS}
    PROPERTIES LANGUAGE CXX
)

target_include_directories(pg_onvif
    PUBLIC  include
)

target_link_libraries(pg_onvif PUBLIC gsoap hal_camera)