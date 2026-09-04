cmake_minimum_required(VERSION 3.10.0)
project(tinyDSP)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(tinyDSP INTERFACE)

# target_link_libraries(tinyDSP INTERFACE rtos-cpp)
target_include_directories(embedded_hal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(embedded_hal INTERFACE
    freertos_kernel
    FreeRTOS-Cpp
)