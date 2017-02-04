set(SRCS
  AudioCommon.cpp
  DPL2Decoder.cpp
  Mixer.cpp
  WaveFile.cpp
  NullSoundStream.cpp
)

add_dolphin_library(audiocommon "${SRCS}" "")

find_package(OpenSLES)
if(OPENSLES_FOUND)
  message(STATUS "OpenSLES found, enabling OpenSLES sound backend")
  target_sources(audiocommon PRIVATE OpenSLESStream.cpp)
  target_link_libraries(audiocommon PRIVATE OpenSLES::OpenSLES)
endif()

if(ALSA_FOUND)
  target_sources(audiocommon PRIVATE AlsaSoundStream.cpp)
  target_link_libraries(audiocommon PRIVATE ${ALSA_LIBRARIES})
endif()

if(AO_FOUND)
  target_sources(audiocommon PRIVATE AOSoundStream.cpp)
  target_link_libraries(audiocommon PRIVATE ${AO_LIBRARIES})
endif()

if(OPENAL_FOUND)
  target_sources(audiocommon PRIVATE OpenALStream.cpp aldlist.cpp)
  target_link_libraries(audiocommon PRIVATE ${OPENAL_LIBRARY} SoundTouch)
endif()

if(PULSEAUDIO_FOUND)
  target_sources(audiocommon PRIVATE PulseAudioStream.cpp)
  target_link_libraries(audiocommon PRIVATE ${PULSEAUDIO_LIBRARIES})
endif()

if(WIN32)
  target_sources(audiocommon PRIVATE XAudio2Stream.cpp)

  add_dolphin_library(audiocommon_xaudio27 "XAudio2_7Stream.cpp" "${LIBS}")
  target_include_directories(audiocommon_xaudio27 PRIVATE
    ${PROJECT_SOURCE_DIR}/Externals
    ${PROJECT_SOURCE_DIR}/Externals/XAudio2_7
  )
  target_link_libraries(audiocommon PRIVATE audiocommon_xaudio27)

elseif(APPLE)
  target_sources(audiocommon PRIVATE CoreAudioSoundStream.cpp)
endif()


