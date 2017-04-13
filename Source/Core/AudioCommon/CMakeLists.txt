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
