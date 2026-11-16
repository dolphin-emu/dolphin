add_library(audiocommon
  AudioCommon.cpp
  AudioCommon.h
  CubebStream.h
  Enums.h
  Microphone.cpp
  Microphone.h
  Mixer.cpp
  Mixer.h
  SurroundDecoder.cpp
  SurroundDecoder.h
  NullSoundStream.cpp
  NullSoundStream.h
  WaveFile.cpp
  WaveFile.h
)

if(ENABLE_CUBEB)
  message(STATUS "Cubeb found, enabling Cubeb sound backend")
  target_sources(audiocommon PRIVATE
    CubebStream.cpp
    CubebUtils.cpp
    CubebUtils.h
  )
  target_link_libraries(audiocommon PRIVATE cubeb)
endif()

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
  FreeSurround)

if(ENABLE_CUBEB)
  target_link_libraries(audiocommon PRIVATE cubeb::cubeb)
endif()

if(MSVC)
  # Add precompiled header
  target_link_libraries(audiocommon PRIVATE use_pch)
endif()
