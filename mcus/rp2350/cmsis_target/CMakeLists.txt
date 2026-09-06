add_library(cmsis_rp2350 INTERFACE)

set(CMSISCORE "${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core" CACHE PATH "CMSIS Core Path" FORCE)

target_compile_definitions(cmsis_rp2350 INTERFACE
    ARM_MATH_CM33=1
    __FPU_PRESENT=1U
)

# target_include_directories(cmsis_rp2350 INTERFACE
#     ${CMSISCORE}/Include
# )