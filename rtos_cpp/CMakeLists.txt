cmake_minimum_required(VERSION 3.10.0)
project(rtos-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(rtos-cpp INTERFACE)
target_link_libraries(rtos-cpp INTERFACE
    rtos
)

target_include_directories(rtos-cpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)