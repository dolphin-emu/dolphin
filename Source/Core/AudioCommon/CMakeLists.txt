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
