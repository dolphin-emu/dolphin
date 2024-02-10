add_library(audiocommon
  AudioCommon.cpp
  AudioCommon.h
  AudioStretcher.cpp
  AudioStretcher.h
  CubebStream.cpp
  CubebStream.h
  CubebUtils.cpp
  CubebUtils.h
  Enums.h
  Mixer.cpp
  Mixer.h
  SurroundDecoder.cpp
  SurroundDecoder.h
  NullSoundStream.cpp
  NullSoundStream.h
  WaveFile.cpp
  WaveFile.h
)

if(ANDROID)
  find_package(OpenSLES)
  if(OPENSLES_FOUND)
    message(STATUS "OpenSLES found, enabling OpenSLES sound backend")
    target_sources(audiocommon PRIVATE
      OpenSLESStream.cpp
      OpenSLESStream.h
    )
    target_link_libraries(audiocommon PRIVATE OpenSLES::OpenSLES)
    target_compile_definitions(audiocommon PRIVATE HAVE_OPENSL_ES=1)
  endif()
endif()

if(ENABLE_ALSA)
  find_package(ALSA)
  if(ALSA_FOUND)
    message(STATUS "ALSA found, enabling ALSA sound backend")
    target_sources(audiocommon PRIVATE
      AlsaSoundStream.cpp
      AlsaSoundStream.h
    )
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
  if(PulseAudio_FOUND)
    message(STATUS "PulseAudio found, enabling PulseAudio sound backend")
    target_sources(audiocommon PRIVATE
      PulseAudioStream.cpp
      PulseAudioStream.h
    )
    target_link_libraries(audiocommon PRIVATE PulseAudio::PulseAudio)
    target_compile_definitions(audiocommon PRIVATE HAVE_PULSEAUDIO=1)
  else()
    message(STATUS "PulseAudio NOT found, disabling PulseAudio sound backend")
  endif()
else()
  message(STATUS "PulseAudio explicitly disabled, disabling PulseAudio sound backend")
endif()

if(WIN32)
  target_sources(audiocommon PRIVATE
    # Dolphin loads openal32.dll at runtime
    OpenALStream.cpp
    OpenALStream.h

    WASAPIStream.cpp
    WASAPIStream.h
  )
endif()

target_link_libraries(audiocommon
PUBLIC
  common

PRIVATE
  cubeb::cubeb
  SoundTouch
  FreeSurround)

if(MSVC)
  # Add precompiled header
  target_link_libraries(audiocommon PRIVATE use_pch)
endif()
