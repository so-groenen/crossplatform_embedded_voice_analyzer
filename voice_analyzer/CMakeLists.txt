add_library(voice_analyzer INTERFACE)

target_include_directories(voice_analyzer INTERFACE
    include/
)

# Link CMSIS-DSP and selected MCU target
target_link_libraries(voice_analyzer INTERFACE
    cmsis_target
    CMSISDSP
)