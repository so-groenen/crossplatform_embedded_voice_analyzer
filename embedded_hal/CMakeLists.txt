cmake_minimum_required(VERSION 3.10.0)
project(embedded_hal)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(embedded_hal INTERFACE)

target_link_libraries(embedded_hal INTERFACE rtos-cpp)
target_include_directories(embedded_hal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)