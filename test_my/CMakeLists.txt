cmake_minimum_required(VERSION 3.6)

project(test_my)

set(CMAKE_CXX_STANDARD 14)

set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(${PROJECT_NAME}
  ${CURRENT_DIR}/main.cc
)

link_directories(${CURRENT_DIR}/../lib)

target_link_libraries(${PROJECT_NAME} mywebrtc dl)
