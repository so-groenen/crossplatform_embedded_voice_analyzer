
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(PICO_SDK_PATH   "${USERHOME}/.pico-sdk/sdk/${sdkVersion}")
set(PICO_TOOLCHAIN_PATH "${USERHOME}/.pico-sdk/toolchain/${toolchainVersion}")

if (sdkVersion VERSION_LESS "2.0.0")
    if(WIN32)
        set(pico-sdk-tools_DIR "${USERHOME}/.pico-sdk/tools/${sdkVersion}")
        include(${pico-sdk-tools_DIR}/pico-sdk-tools-config.cmake)
        include(${pico-sdk-tools_DIR}/pico-sdk-tools-config-version.cmake)
    endif()
else()
    set(pioasm_HINT "${USERHOME}/.pico-sdk/tools/${sdkVersion}/pioasm")
    if(EXISTS ${pioasm_HINT})
        set(pioasm_DIR ${pioasm_HINT})
    endif()
    set(picotool_HINT "${USERHOME}/.pico-sdk/picotool/${picotoolVersion}/picotool")
    if(EXISTS ${picotool_HINT})
        set(picotool_DIR ${picotool_HINT})
    endif()
    if(PICO_TOOLCHAIN_PATH MATCHES "RISCV")
        set(PICO_PLATFORM rp2350-riscv CACHE STRING "Pico Platform")
        if((PICO_TOOLCHAIN_PATH MATCHES "RISCV_ZCB") AND (sdkVersion VERSION_LESS "2.3.0"))
            set(PICO_COMPILER "pico_riscv_gcc_zcb_zcmp")
        endif()
    endif()
endif()




if(${PICO_BOARD} STREQUAL "pico_w")
    set(FREERTOS_KERNEL_PORT_PATH "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040")
elseif(${PICO_BOARD} STREQUAL "pico2" OR ${PICO_BOARD} STREQUAL "pico2_w")
    set(FREERTOS_KERNEL_PORT_PATH "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ")
else()
    message(WARNING "Unrecognized PICO_BOARD value: ${PICO_BOARD}")
endif()



include(${RP2350_DIR}/pico_sdk_import.cmake)
include(${FREERTOS_KERNEL_PORT_PATH}/library.cmake)
message(DEBUG "Using port: ${FREERTOS_KERNEL_PORT_PATH}")

# Create alias so that libs depending on freertos_kernel can find it, because the rp2350 uses "FreeRTOS-Kernel" as target
add_library(freertos_kernel ALIAS FreeRTOS-Kernel)


if(${PICO_BOARD} STREQUAL "pico_w")
    target_link_libraries(${OUTPUT_NAME} pico_cyw43_arch_none)
    target_compile_definitions(FreeRTOS-Kernel INTERFACE PICO_RP2040=1)
endif()

if(${PICO_BOARD} STREQUAL "pico2_w")
    target_compile_definitions(FreeRTOS-Kernel INTERFACE PICO_RP2350=1)
endif()

if(${PICO_BOARD} STREQUAL "pico2")
    target_compile_definitions(FreeRTOS-Kernel INTERFACE PICO_RP2350=1)
endif()