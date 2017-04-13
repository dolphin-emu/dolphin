set(SRCS
  AudioCommon.cpp
  CubebStream.cpp
  DPL2Decoder.cpp
  Mixer.cpp
  WaveFile.cpp
  NullSoundStream.cpp
)
set(LIBS
  cubeb
  SoundTouch
)

add_dolphin_library(audiocommon "${SRCS}" "${LIBS}")

find_package(OpenSLES)
if(OPENSLES_FOUND)
  message(STATUS "OpenSLES found, enabling OpenSLES sound backend")
  target_sources(audiocommon PRIVATE OpenSLESStream.cpp)
  target_link_libraries(audiocommon PRIVATE OpenSLES::OpenSLES)
endif()

if(ENABLE_ALSA)
  find_package(ALSA)
  if(ALSA_FOUND)
    message(STATUS "ALSA found, enabling ALSA sound backend")
    target_sources(audiocommon PRIVATE AlsaSoundStream.cpp)
    target_link_libraries(audiocommon PRIVATE ALSA::ALSA)
    target_compile_definitions(audiocommon PRIVATE HAVE_ALSA=1)
  else()
    message(STATUS "ALSA NOT found, disabling ALSA sound backend")
  endif()
else()
  message(STATUS "ALSA explicitly disabled, disabling ALSA sound backend")
endif()
