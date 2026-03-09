# RK3588S platform exported headers.

set(PG_RK3588S_SDK_INCLUDE_DIR "" CACHE PATH "Root include dir for RK3588S media SDK")
set(RK3588S_BSP_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/bsp/include)

find_path(RK3588S_MPI_INCLUDE_DIR
	NAMES rk_mpi_vi.h
	PATHS
		${PG_RK3588S_SDK_INCLUDE_DIR}
		${RK3588S_BSP_INCLUDE_ROOT}
	PATH_SUFFIXES
		""
		rk_media
		rkmedia
)

if(NOT RK3588S_MPI_INCLUDE_DIR)
	message(FATAL_ERROR
		"PG_HAL_USE_RK3588S=ON but rk_mpi_vi.h was not found. "
		"Set PG_RK3588S_SDK_INCLUDE_DIR to your RK media SDK include path.")
endif()

add_library(pg_platform_rk3588s_headers INTERFACE)

target_include_directories(pg_platform_rk3588s_headers
	INTERFACE
		${RK3588S_BSP_INCLUDE_ROOT}
		${RK3588S_BSP_INCLUDE_ROOT}/rk_media
		${RK3588S_BSP_INCLUDE_ROOT}/rkmedia
		${RK3588S_MPI_INCLUDE_DIR}
)

