add_library(hal_camera STATIC)

set(HAL_CAMERA_IMPL_SRC src/hal_camera_stub.cpp)

target_include_directories(hal_camera
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(PG_HAL_USE_RK3588S)
	set(HAL_CAMERA_IMPL_SRC ${CMAKE_SOURCE_DIR}/platform/rk3588s/port/src/hal_camera_port.c)

	if(NOT TARGET pg_platform_rk3588s_headers)
		message(FATAL_ERROR "Missing target pg_platform_rk3588s_headers. Ensure platform/rk3588s is added before components/hal.")
	endif()

	target_include_directories(hal_camera
		PRIVATE
			${CMAKE_SOURCE_DIR}/platform/rk3588s/port/include
			${CMAKE_SOURCE_DIR}/components/utils/log/include
			$<TARGET_PROPERTY:pg_platform_rk3588s_headers,INTERFACE_INCLUDE_DIRECTORIES>
	)

	target_compile_definitions(hal_camera PRIVATE PG_HAL_IMPL_RK3588S=1)
else()
	target_compile_definitions(hal_camera PRIVATE PG_HAL_IMPL_STUB=1)
endif()

target_sources(hal_camera
	PRIVATE
		${HAL_CAMERA_IMPL_SRC}
)
