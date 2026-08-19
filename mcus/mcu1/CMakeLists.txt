cmake_minimum_required(VERSION 3.10.0)
project(mcu_example1 VERSION 0.1.0 LANGUAGES C CXX)

add_executable(mcu_example1 main.cpp)

target_link_libraries(mcu_example1 PRIVATE rtos)
target_link_libraries(mcu_example1 PRIVATE rtos-cpp)
target_link_libraries(mcu_example1 PRIVATE embedded_hal)

target_include_directories(mcu_example1 PRIVATE driver)

target_compile_features(mcu_example1 PRIVATE cxx_std_23)

