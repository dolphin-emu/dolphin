# TODO
# - backend selection via command line, rather than simply detecting headers.

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(cubeb
  VERSION 0.0.0)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_RUST_LIBS "Build rust backends" OFF)
option(BUILD_TOOLS "Build tools" ON)

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

if(BUILD_TESTS)
  if(NOT TARGET gtest_main)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/googletest/CMakeLists.txt")
      message(FATAL_ERROR "Could not find googletest: run\n\tgit submodule update --init --recursive\nin base git checkout")
    endif()
    add_definitions(-DGTEST_HAS_TR1_TUPLE=0)
    set(gtest_force_shared_crt ON CACHE BOOL "")
    add_subdirectory(googletest)
  endif()
endif()

if (BUILD_RUST_LIBS)
  if(EXISTS "${PROJECT_SOURCE_DIR}/src/cubeb-pulse-rs")
    set(USE_PULSE_RUST 1)
  endif()
  if(EXISTS "${PROJECT_SOURCE_DIR}/src/cubeb-coreaudio-rs")
    set(USE_AUDIOUNIT_RUST 1)
  endif()
endif()

# On OS/2, visibility attribute is not supported.
if(NOT OS2)
  set(CMAKE_C_VISIBILITY_PRESET hidden)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
endif()

set(CMAKE_CXX_WARNING_LEVEL 4)
if(NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
endif()

add_library(cubeb
  src/cubeb.c
  src/cubeb_mixer.cpp
  src/cubeb_resampler.cpp
  src/cubeb_log.cpp
  src/cubeb_strings.c
  src/cubeb_utils.cpp
   $<TARGET_OBJECTS:speex>)
target_include_directories(cubeb
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
)
target_include_directories(cubeb PRIVATE src)
target_compile_definitions(cubeb PRIVATE OUTSIDE_SPEEX)
target_compile_definitions(cubeb PRIVATE FLOATING_POINT)
target_compile_definitions(cubeb PRIVATE EXPORT=)
target_compile_definitions(cubeb PRIVATE RANDOM_PREFIX=speex)

add_sanitizers(cubeb)

include(GenerateExportHeader)
generate_export_header(cubeb EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/exports/cubeb_export.h)
target_include_directories(cubeb
  PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/exports>
)

if(UNIX)
  include(GNUInstallDirs)
else()
  set(CMAKE_INSTALL_LIBDIR "lib")
  set(CMAKE_INSTALL_BINDIR "bin")
  set(CMAKE_INSTALL_DATADIR "share")
  set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATADIR}/doc")
  set(CMAKE_INSTALL_INCLUDEDIR "include")
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${CMAKE_BINARY_DIR}/exports/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "Config.cmake.in"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(TARGETS cubeb
  EXPORT "${PROJECT_NAME}Targets"
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
  FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)
install(
  EXPORT "${PROJECT_NAME}Targets"
  NAMESPACE "${PROJECT_NAME}::"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

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
  target_link_libraries(cubeb PRIVATE pthread ${CMAKE_DL_LIBS})
endif()

check_include_files(alsa/asoundlib.h USE_ALSA)
if(USE_ALSA)
  target_sources(cubeb PRIVATE
    src/cubeb_alsa.c)
  target_compile_definitions(cubeb PRIVATE USE_ALSA)
  target_link_libraries(cubeb PRIVATE pthread ${CMAKE_DL_LIBS})
endif()

check_include_files(jack/jack.h USE_JACK)
if(USE_JACK)
  target_sources(cubeb PRIVATE
    src/cubeb_jack.cpp)
  target_compile_definitions(cubeb PRIVATE USE_JACK)
  target_link_libraries(cubeb PRIVATE pthread ${CMAKE_DL_LIBS})
endif()

check_include_files(audioclient.h USE_WASAPI)
if(USE_WASAPI)
  target_sources(cubeb PRIVATE
    src/cubeb_wasapi.cpp)
  target_compile_definitions(cubeb PRIVATE USE_WASAPI)
  target_link_libraries(cubeb PRIVATE avrt ole32)
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
    src/cubeb_opensl.c
    src/cubeb-jni.cpp)
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
  target_link_libraries(cubeb PRIVATE pthread ${CMAKE_DL_LIBS})
endif()

check_include_files(sys/audioio.h USE_SUN)
if(USE_SUN)
  target_sources(cubeb PRIVATE
    src/cubeb_sun.c)
  target_compile_definitions(cubeb PRIVATE USE_SUN)
  target_link_libraries(cubeb PRIVATE pthread)
endif()

check_include_files(kai.h USE_KAI)
if(USE_KAI)
  target_sources(cubeb PRIVATE
    src/cubeb_kai.c)
  target_compile_definitions(cubeb PRIVATE USE_KAI)
  target_link_libraries(cubeb PRIVATE kai)
endif()

if(USE_PULSE_RUST)
  include(ExternalProject)
  set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/rust)
  ExternalProject_Add(
    cubeb_pulse_rs
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build COMMAND cargo build --release
    BUILD_ALWAYS ON
    BINARY_DIR "${CMAKE_SOURCE_DIR}/src/cubeb-pulse-rs"
    INSTALL_COMMAND ""
    LOG_BUILD ON)
  add_dependencies(cubeb cubeb_pulse_rs)
  target_compile_definitions(cubeb PRIVATE USE_PULSE_RUST)
  target_link_libraries(cubeb PRIVATE
    debug "${CMAKE_SOURCE_DIR}/src/cubeb-pulse-rs/target/debug/libcubeb_pulse.a"
    optimized "${CMAKE_SOURCE_DIR}/src/cubeb-pulse-rs/target/release/libcubeb_pulse.a" pulse)
endif()

if(USE_AUDIOUNIT_RUST)
  include(ExternalProject)
  set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/rust)
  ExternalProject_Add(
    cubeb_coreaudio_rs
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build COMMAND cargo build --release
    BUILD_ALWAYS ON
    BINARY_DIR "${CMAKE_SOURCE_DIR}/src/cubeb-coreaudio-rs"
    INSTALL_COMMAND ""
    LOG_BUILD ON)
  add_dependencies(cubeb cubeb_coreaudio_rs)
  target_compile_definitions(cubeb PRIVATE USE_AUDIOUNIT_RUST)
  target_link_libraries(cubeb PRIVATE
    debug "${CMAKE_SOURCE_DIR}/src/cubeb-coreaudio-rs/target/debug/libcubeb_coreaudio.a"
    optimized "${CMAKE_SOURCE_DIR}/src/cubeb-coreaudio-rs/target/release/libcubeb_coreaudio.a")
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile @ONLY)
  add_custom_target(doc ALL
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs
    COMMENT "Generating API documentation with Doxygen" VERBATIM)
endif()

if(BUILD_TESTS)
  enable_testing()

  macro(cubeb_add_test NAME)
    add_executable(test_${NAME} test/test_${NAME}.cpp)
    target_include_directories(test_${NAME} PRIVATE ${gtest_SOURCE_DIR}/include)
    target_include_directories(test_${NAME} PRIVATE src)
    target_link_libraries(test_${NAME} PRIVATE cubeb gtest_main)
    add_test(${NAME} test_${NAME})
    add_sanitizers(test_${NAME})
    install(TARGETS test_${NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR})
  endmacro(cubeb_add_test)

  cubeb_add_test(sanity)
  cubeb_add_test(tone)
  cubeb_add_test(audio)
  cubeb_add_test(record)
  cubeb_add_test(devices)
  cubeb_add_test(callback_ret)

  add_executable(test_resampler test/test_resampler.cpp src/cubeb_resampler.cpp $<TARGET_OBJECTS:speex>)
  target_include_directories(test_resampler PRIVATE ${gtest_SOURCE_DIR}/include)
  target_include_directories(test_resampler PRIVATE src)
  target_compile_definitions(test_resampler PRIVATE OUTSIDE_SPEEX)
  target_compile_definitions(test_resampler PRIVATE FLOATING_POINT)
  target_compile_definitions(test_resampler PRIVATE EXPORT=)
  target_compile_definitions(test_resampler PRIVATE RANDOM_PREFIX=speex)
  target_link_libraries(test_resampler PRIVATE cubeb gtest_main)
  add_test(resampler test_resampler)
  add_sanitizers(test_resampler)
  install(TARGETS test_resampler DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR})

  cubeb_add_test(duplex)

  if (USE_WASAPI)
    cubeb_add_test(overload_callback)
    cubeb_add_test(loopback)
  endif()

  cubeb_add_test(latency test_latency)
  cubeb_add_test(ring_array)

  cubeb_add_test(utils)
  cubeb_add_test(ring_buffer)
  cubeb_add_test(device_changed_callback)
endif()

if(BUILD_TOOLS)
  add_executable(cubeb-test tools/cubeb-test.cpp)
  target_include_directories(cubeb-test PRIVATE src)
  target_link_libraries(cubeb-test PRIVATE cubeb)
  add_sanitizers(cubeb-test)
  install(TARGETS cubeb-test DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR})
endif()
