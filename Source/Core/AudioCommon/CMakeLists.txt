set(SRCS
  AudioCommon.cpp
  AudioStretcher.cpp
  CubebStream.cpp
  CubebUtils.cpp
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

if(ENABLE_PULSEAUDIO)
  # PulseAudio ships with a PulseAudioConfig.cmake with no imported target
  # So we use our own FindPulseAudio instead with "MODULE"
  find_package(PulseAudio MODULE QUIET)
  if(PULSEAUDIO_FOUND)
    message(STATUS "PulseAudio found, enabling PulseAudio sound backend")
    target_sources(audiocommon PRIVATE PulseAudioStream.cpp)
    target_link_libraries(audiocommon PRIVATE PulseAudio::PulseAudio)
    target_compile_definitions(audiocommon PRIVATE HAVE_PULSEAUDIO=1)
  else()
    message(STATUS "PulseAudio NOT found, disabling PulseAudio sound backend")
  endif()
else()
  message(STATUS "PulseAudio explicitly disabled, disabling PulseAudio sound backend")
endif()

if(WIN32)
  target_sources(audiocommon PRIVATE XAudio2Stream.cpp)

  add_dolphin_library(audiocommon_xaudio27 "XAudio2_7Stream.cpp" "${LIBS}")
  target_include_directories(audiocommon_xaudio27 PRIVATE
    ${PROJECT_SOURCE_DIR}/Externals
    ${PROJECT_SOURCE_DIR}/Externals/XAudio2_7
  )
  target_link_libraries(audiocommon PRIVATE audiocommon_xaudio27)

  set(ENV{OPENALDIR} ${PROJECT_SOURCE_DIR}/Externals/OpenAL)
  find_package(OpenAL)
  if(OPENAL_FOUND)
    message(STATUS "OpenAL found, enabling OpenAL sound backend")
    target_sources(audiocommon PRIVATE OpenALStream.cpp)
    target_link_libraries(audiocommon PRIVATE OpenAL::OpenAL)
  else()
    message(FATAL_ERROR "OpenAL NOT found in Externals")
  endif()
endif()

target_link_libraries(audiocommon PRIVATE cubeb SoundTouch)
