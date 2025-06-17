cmake_minimum_required(VERSION 3.6)

project(libmy)

set(CMAKE_CXX_STANDARD 14)

set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${CURRENT_DIR}/../webrtc)

add_library(${PROJECT_NAME} SHARED
  ${CURRENT_DIR}/main.cc
)

target_link_libraries(${PROJECT_NAME} webrtc_apm)
