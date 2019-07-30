add_library(audiocommon
  AudioCommon.cpp
  AudioCommon.h
  AudioStretcher.cpp
  AudioStretcher.h
  CubebStream.cpp
  CubebStream.h
  CubebUtils.cpp
  CubebUtils.h
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
  set(OPENSLES_FOUND 1)
  message(STATUS "OpenSLES found, enabling OpenSLES sound backend")
  target_sources(audiocommon PRIVATE
    OpenSLESStream.cpp
    OpenSLESStream.h
  )
  target_link_libraries(audiocommon PRIVATE OpenSLES)
else()
  find_package(OpenSLES)
  if(OPENSLES_FOUND)
    message(STATUS "OpenSLES found, enabling OpenSLES sound backend")
    target_sources(audiocommon PRIVATE
      OpenSLESStream.cpp
      OpenSLESStream.h
    )
    target_link_libraries(audiocommon PRIVATE OpenSLES::OpenSLES)
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
  if(PULSEAUDIO_FOUND)
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
    XAudio2Stream.cpp
    XAudio2Stream.h
  )

  add_library(audiocommon_xaudio27
    XAudio2_7Stream.cpp
    XAudio2_7Stream.h
  )
  target_include_directories(audiocommon_xaudio27 PRIVATE
    ${PROJECT_SOURCE_DIR}/Externals
    ${PROJECT_SOURCE_DIR}/Externals/XAudio2_7
  )
  target_link_libraries(audiocommon PRIVATE audiocommon_xaudio27)
endif()

target_link_libraries(audiocommon PRIVATE cubeb SoundTouch FreeSurround)
