cmake_minimum_required(VERSION 3.12)

set(PROJECT_NAME rp2350_example)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(${PROJECT_NAME} LANGUAGES C CXX ASM)

pico_sdk_init()

 
add_executable(rp2350_example 
    src/main.cpp)

target_include_directories(rp2350_example PUBLIC 
    ${FREERTOS_KERNEL_PATH}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/config 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

pico_enable_stdio_usb(rp2350_example 1)


target_link_libraries(rp2350_example 
    pico_stdlib
    pico_runtime
    pico_stdio_usb
    pico_cyw43_arch_none
    FreeRTOS-Kernel-Heap4
    embedded_hal
    FreeRTOS-Cpp)

target_compile_options(rp2350_example PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile>)
target_compile_features(rp2350_example PRIVATE cxx_std_23)


pico_add_extra_outputs(rp2350_example)