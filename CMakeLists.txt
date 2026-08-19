cmake_minimum_required(VERSION 3.10.0)
project(CrossPlatformProject VERSION 0.1.0 LANGUAGES C CXX)

# add_executable(my_proj main.cpp)

add_library(freertos_config INTERFACE)



if(PLATFORM STREQUAL "MCU1")
    message(DEBUG "MCU 1")
elseif(PLATFORM STREQUAL "MCU2")
    message(DEBUG "MCU 2")
elseif(PLATFORM STREQUAL "UNIX")
    message(DEBUG "UNIX")

    target_include_directories(freertos_config INTERFACE 
        "${CMAKE_CURRENT_SOURCE_DIR}/mcus/unix"
    )
    set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "FreeRTOS Port Target")
    set(FREERTOS_HEAP "4" CACHE STRING "FreeRTOS Heap Implementation")
endif()


# RTOS C 
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rtos)
# target_link_libraries(my_proj PRIVATE rtos)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Cpp/FreeRTOS-Cpp)
# RTOS CPP
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rtos_cpp)
# target_link_libraries(my_proj PRIVATE rtos-cpp)

# API (Concepts + driver)
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/embedded_hal)

if(PLATFORM STREQUAL "MCU1")
    add_subdirectory(mcus/mcu1)
elseif(PLATFORM STREQUAL "MCU2")
    add_subdirectory(mcus/mcu2)
elseif(PLATFORM STREQUAL "UNIX")
    add_subdirectory(mcus/unix)
endif()

 

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)