cmake_minimum_required(VERSION 3.10.0)

# FreeRTOS target so it can find the correct config file
add_library(freertos_config INTERFACE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel")
set(VOICE_ANALYZER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/voice_analyzer")

set(FREERTOS_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}//FreeRTOS-Cpp/FreeRTOS-Cpp")
set(RP2350_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mcus/rp2350")
set(UNIX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mcus/unix")

#------------------------------------------------------------#
#       Setting FreeRTOS ports depending on target arch!     #
#------------------------------------------------------------#

if(PLATFORM STREQUAL "MCU1")
    
    message(DEBUG "MCU 1")

elseif(PLATFORM STREQUAL "RP2350")

    message(DEBUG "Using RP2350 port")
    
    # Choose your board & versions!
    set(PICO_BOARD pico2_w CACHE STRING "Board type")

    set(sdkVersion 2.3.0)
    set(toolchainVersion 15_2_Rel1)
    set(picotoolVersion 2.3.0)

    # set(sdkVersion 2.2.0)
    # set(toolchainVersion 14_2_Rel1)
    # set(picotoolVersion 2.2.0-a4)
    
    target_include_directories(freertos_config SYSTEM INTERFACE  
        "${RP2350_DIR}/config"
    )
    include(pico-freertos-setup.cmake)
    
elseif(PLATFORM STREQUAL "UNIX")
    
    message(DEBUG "Using UNIX port")
    target_include_directories(freertos_config SYSTEM INTERFACE  
        "${UNIX_DIR}/config"
    )
    set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "FreeRTOS Port Target")
    set(FREERTOS_HEAP "4" CACHE STRING "FreeRTOS Heap Implementation")
endif()


# Need to be called AFTER pico stuff, so that the compiler uses the correct target archi!
project(CrossPlatformProject VERSION 0.1.0 LANGUAGES C CXX ASM)

#------------------------------------------------------------#
#         include FreeRTOS & FreeRTOS-Cpp as library         #
#------------------------------------------------------------#
if(NOT PLATFORM STREQUAL "RP2350")
    add_subdirectory(${FREERTOS_KERNEL_PATH})
endif()
 

#------------------------------------------------------------#
#     Embedded-HAL CPP depends on FreerRTOS/FreeRTOS-Cpp     #
#------------------------------------------------------------#
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/embedded_hal)



#------------------------------------------------------------#
#                   Add implementations                      #
#------------------------------------------------------------#
add_library(cmsis_target INTERFACE)
if(PLATFORM STREQUAL "MCU1")

    add_subdirectory(mcus/mcu1)

elseif(PLATFORM STREQUAL "RP2350")
    add_subdirectory(mcus/rp2350/cmsis_target)
    target_link_libraries(cmsis_target INTERFACE cmsis_rp2350)
    add_subdirectory(${RP2350_DIR})
elseif(PLATFORM STREQUAL "UNIX")
    add_subdirectory(${UNIX_DIR})
endif()

#------------------------------------------------------------#
#     Fetch CMSIS-DSP                                        #
#------------------------------------------------------------#
include(CMSIS-DSP/CMSIS-DSP.cmake)


#------------------------------------------------------------#
#     FreeRTOS-Cpp                                           #
#------------------------------------------------------------#
add_subdirectory(${FREERTOS_CPP_PATH})

 #------------------------------------------------------------#
#     VOICE_ANALYZER_PATH                                                #
#------------------------------------------------------------#
add_subdirectory(${VOICE_ANALYZER_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)