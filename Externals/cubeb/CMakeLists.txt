# TODO
# - backend selection via command line, rather than simply detecting headers.

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(cubeb
  VERSION 0.0.0)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT COMMAND add_sanitizers)
  list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/sanitizers-cmake/cmake")
  find_package(Sanitizers)
  if(NOT COMMAND add_sanitizers)
    message(FATAL_ERROR "Could not find sanitizers-cmake: run\n\tgit submodule update --init --recursive\nin base git checkout")
  endif()
endif()

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(CMAKE_CXX_WARNING_LEVEL 4)
if(NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
endif()

add_library(cubeb
  src/cubeb.c
  src/cubeb_mixer.cpp
  src/cubeb_resampler.cpp
  src/cubeb_panner.cpp
  src/cubeb_log.cpp
  src/cubeb_utils.c
   $<TARGET_OBJECTS:speex>)
target_include_directories(cubeb PUBLIC include)
target_include_directories(cubeb PRIVATE src)
target_compile_definitions(cubeb PRIVATE OUTSIDE_SPEEX)
target_compile_definitions(cubeb PRIVATE FLOATING_POINT)
target_compile_definitions(cubeb PRIVATE EXPORT=)
target_compile_definitions(cubeb PRIVATE RANDOM_PREFIX=speex)

add_sanitizers(cubeb)

include(GenerateExportHeader)
generate_export_header(cubeb EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/exports/cubeb_export.h)
target_include_directories(cubeb PUBLIC ${CMAKE_BINARY_DIR}/exports)

add_library(speex OBJECT
  src/speex/resample.c)
set_target_properties(speex PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_compile_definitions(speex PRIVATE OUTSIDE_SPEEX)
target_compile_definitions(speex PRIVATE FLOATING_POINT)
target_compile_definitions(speex PRIVATE EXPORT=)
target_compile_definitions(speex PRIVATE RANDOM_PREFIX=speex)

include(CheckIncludeFiles)

check_include_files(AudioUnit/AudioUnit.h USE_AUDIOUNIT)
if(USE_AUDIOUNIT)
  target_sources(cubeb PRIVATE
    src/cubeb_audiounit.cpp
    src/cubeb_osx_run_loop.cpp)
  target_compile_definitions(cubeb PRIVATE USE_AUDIOUNIT)
  target_link_libraries(cubeb PRIVATE "-framework AudioUnit" "-framework CoreAudio" "-framework CoreServices")
endif()

check_include_files(pulse/pulseaudio.h USE_PULSE)
if(USE_PULSE)
  target_sources(cubeb PRIVATE
    src/cubeb_pulse.c)
  target_compile_definitions(cubeb PRIVATE USE_PULSE)
  if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(cubeb PRIVATE dl)
  endif()
endif()

check_include_files(alsa/asoundlib.h USE_ALSA)
if(USE_ALSA)
  target_sources(cubeb PRIVATE
    src/cubeb_alsa.c)
  target_compile_definitions(cubeb PRIVATE USE_ALSA)
  target_link_libraries(cubeb PRIVATE asound pthread)
endif()

check_include_files(jack/jack.h USE_JACK)
if(USE_JACK)
  target_sources(cubeb PRIVATE
    src/cubeb_jack.cpp)
  target_compile_definitions(cubeb PRIVATE USE_JACK)
  if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(cubeb PRIVATE dl)
  endif()
  target_link_libraries(cubeb PRIVATE pthread)
endif()

check_include_files(audioclient.h USE_WASAPI)
if(USE_WASAPI)
  target_sources(cubeb PRIVATE
    src/cubeb_wasapi.cpp)
  target_compile_definitions(cubeb PRIVATE USE_WASAPI)
  target_link_libraries(cubeb PRIVATE avrt)
endif()

check_include_files("windows.h;mmsystem.h" USE_WINMM)
if(USE_WINMM)
  target_sources(cubeb PRIVATE
    src/cubeb_winmm.c)
  target_compile_definitions(cubeb PRIVATE USE_WINMM)
  target_link_libraries(cubeb PRIVATE winmm)
endif()

check_include_files(SLES/OpenSLES.h USE_OPENSL)
if(USE_OPENSL)
  target_sources(cubeb PRIVATE
    src/cubeb_opensl.c)
  target_compile_definitions(cubeb PRIVATE USE_OPENSL)
  target_link_libraries(cubeb PRIVATE OpenSLES)
endif()

check_include_files(android/log.h USE_AUDIOTRACK)
if(USE_AUDIOTRACK)
  target_sources(cubeb PRIVATE
    src/cubeb_audiotrack.c)
  target_compile_definitions(cubeb PRIVATE USE_AUDIOTRACK)
  target_link_libraries(cubeb PRIVATE log)
endif()

check_include_files(sndio.h USE_SNDIO)
if(USE_SNDIO)
  target_sources(cubeb PRIVATE
    src/cubeb_sndio.c)
  target_compile_definitions(cubeb PRIVATE USE_SNDIO)
  target_link_libraries(cubeb PRIVATE sndio)
endif()

check_include_files(kai.h USE_KAI)
if(USE_KAI)
  target_sources(cubeb PRIVATE
    src/cubeb_kai.c)
  target_compile_definitions(cubeb PRIVATE USE_KAI)
  target_link_libraries(cubeb PRIVATE kai)
endif()
