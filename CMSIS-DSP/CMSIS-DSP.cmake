include(FetchContent)


set(CMSIS_DSP_REALFLOAT32 ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(DISABLEFLOAT16 ON CACHE BOOL "Disable float16 kernels in CMSIS-DSP")

# if(NOT TARGET CMSISDSP)
    FetchContent_Declare(
        cmsis_dsp
        GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
        GIT_TAG        v1.17.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(cmsis_dsp)
# endif()