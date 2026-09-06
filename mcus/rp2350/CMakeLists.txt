cmake_minimum_required(VERSION 3.12)

set(PROJECT_NAME rp2350_example)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(${PROJECT_NAME} LANGUAGES C CXX ASM)

pico_sdk_init()

#------------------------------------------------------------#
#     Interface / config lib so i can reuse it for tests     #
#------------------------------------------------------------#
add_library(rp2350_rtos_config INTERFACE)

target_include_directories(rp2350_rtos_config INTERFACE 
    ${FREERTOS_KERNEL_PATH}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/config 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# set(CMSISCORE "${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core" CACHE PATH "CMSIS Core Path")

# include(FetchContent)
# if(NOT TARGET CMSISDSP)
#     FetchContent_Declare(
#         cmsis_dsp
#         GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
#         GIT_TAG        v1.17.1
#         GIT_SHALLOW    TRUE
#     )
#     FetchContent_MakeAvailable(cmsis_dsp)
# endif()

# # Disable float16 functions to avoid compile issues with older compilers and keep build lightweight
# set(DISABLEFLOAT16 ON CACHE BOOL "Disable float16 kernels in CMSIS-DSP")

target_link_libraries(rp2350_rtos_config INTERFACE 
    pico_stdlib
    pico_multicore
    pico_runtime
    pico_stdio_usb
    hardware_pio
    # pico_cyw43_arch_lwip_threadsafe_background
    # pico_cyw43_arch_lwip_sys_freertos
    pico_cyw43_arch_none
    # FreeRTOS-Kernel-Heap4
    embedded_hal
    # FreeRTOS-Cpp
    hardware_spi 
    hardware_dma
    # CMSISDSP
    voice_analyzer 
)
pico_generate_pio_header(rp2350_rtos_config ${CMAKE_CURRENT_LIST_DIR}/src/drivers/i2s.pio)

set(CMAKE_BUILD_TYPE Debug)


target_compile_options(rp2350_rtos_config INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile> -Og -g3)
target_compile_features(rp2350_rtos_config INTERFACE cxx_std_23)


option(BUILD_MAIN "Build the main application target" ON)
option(BUILD_TESTS "Build the example test targets" ON)


#------------------------------------------------------------#
#    Setup pico & add config interface to examples & main    #
#------------------------------------------------------------#
function(configure_picor_rtos_target TARGET_NAME)
    target_link_libraries(${TARGET_NAME} PRIVATE rp2350_rtos_config)
    pico_enable_stdio_usb(${TARGET_NAME} 0)
    pico_enable_stdio_uart(${TARGET_NAME} 1)
    pico_add_extra_outputs(${TARGET_NAME})
    target_compile_definitions(${TARGET_NAME} PRIVATE DEBUG)
endfunction()



#-------------#
#    Drivers    #
#-------------#
set(DRIVER_SRC 
    src/drivers/input.cpp
    src/drivers/async_input.cpp
    # src/drivers/spi.cpp
    )



if(BUILD_MAIN)
    add_executable(rp2350_main 
        src/main.cpp
        ${DRIVER_SRC}
    )
    configure_picor_rtos_target(rp2350_main)
endif()

#-------------#
#    Tests    #
#-------------#
if(BUILD_TESTS)

    add_executable(test_output 
        src/tests/output_test.cpp
        ${DRIVER_SRC}
    )
    configure_picor_rtos_target(test_output)

    
    add_executable(test_exti 
        src/tests/exti_test.cpp 
        ${DRIVER_SRC}
    )
    configure_picor_rtos_target(test_exti)


    add_executable(test_raw_dma  
        src/tests/raw_spi_dma_test.cpp 
        ${DRIVER_SRC}
    )
    configure_picor_rtos_target(test_raw_dma)

    add_executable(test_rtos_spi  
        src/tests/rtos_spi_test.cpp 
        ${DRIVER_SRC}
    )
    configure_picor_rtos_target(test_rtos_spi)

    add_executable(test_embedded_hal
        src/tests/embedded_hal_test.cpp 
        ${DRIVER_SRC}
    )
    configure_picor_rtos_target(test_embedded_hal)

    add_executable(test_uart
        src/tests/uart_dma_test.cpp 
        ${DRIVER_SRC}
    )
    configure_picor_rtos_target(test_uart)



endif()
 