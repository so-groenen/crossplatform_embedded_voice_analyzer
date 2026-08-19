cmake_minimum_required(VERSION 3.10.0)
project(mcu_example2 VERSION 0.1.0 LANGUAGES C CXX)

add_executable(mcu_example2 main.cpp)

# RTOS C 
target_link_libraries(mcu_example2 PRIVATE rtos)
target_link_libraries(mcu_example2 PRIVATE rtos-cpp)

target_compile_features(mcu_example2 PRIVATE cxx_std_23)

