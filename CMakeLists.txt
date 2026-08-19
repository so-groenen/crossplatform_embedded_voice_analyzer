cmake_minimum_required(VERSION 3.10.0)
project(CrossPlatformProject VERSION 0.1.0 LANGUAGES C CXX)

# add_executable(my_proj main.cpp)



# RTOS C 
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rtos)
# target_link_libraries(my_proj PRIVATE rtos)

# RTOS CPP
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rtos_cpp)
# target_link_libraries(my_proj PRIVATE rtos-cpp)

# API (Concepts + driver)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/embedded_hal)

if(PLATFORM STREQUAL "MCU1")
    add_subdirectory(mcus/mcu1)
elseif(PLATFORM STREQUAL "MCU2")
    add_subdirectory(mcus/mcu2)
endif()

 

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)