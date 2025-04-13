set(COMPONENT_SRCS "audio_manager.c")
set(COMPONENT_ADD_INCLUDEDIRS .)
set(COMPONENT_REQUIRES audio_sal audio_stream audio_hal audio_board audio_recorder audio_flash_tone)

register_component()
