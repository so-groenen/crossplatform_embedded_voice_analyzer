cmake_minimum_required(VERSION 3.10.0)
project(rtos VERSION 0.1.0 LANGUAGES)

add_library(rtos STATIC)

target_sources(rtos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/port/rtos.c
)

target_include_directories(rtos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)