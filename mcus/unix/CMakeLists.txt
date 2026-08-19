cmake_minimum_required(VERSION 3.10.0)
project(unix_basic VERSION 0.1.0 LANGUAGES C CXX)

add_executable(unix_basic main.cpp)

# FreeRTOS needs access to its configuration file
target_include_directories(unix_basic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
 
# Link dependencies
target_link_libraries(unix_basic PRIVATE
    freertos_kernel
    FreeRTOS-Cpp
    embedded_hal
    pthread
    rt
)
target_compile_features(unix_basic PRIVATE cxx_std_23)
