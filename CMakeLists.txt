cmake_minimum_required(VERSION 3.6)
project(webrtc_apm_cmake)

set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# if not defined, static lib is generated in default.
set(MYLIB_TYPE SHARED)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE Release)


add_subdirectory(${CURRENT_DIR}/webrtc)

if( NOT ${MYLIB_TYPE})
	# with shared lib
	add_subdirectory(${CURRENT_DIR}/libmy)
	add_subdirectory(${CURRENT_DIR}/test_my)
else()
	# test application with static lib
	add_subdirectory(${CURRENT_DIR}/test_apm)
endif()
