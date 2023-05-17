cmake_minimum_required (VERSION 2.8.12)

project (open-vcdiff CXX C)
set (VERSION 0.8.4)
# For config.h.in to avoid generic define
set (OPEN_VCDIFF_VERSION ${VERSION})
set (PROJECT_SOVERSION 0)


# Configuration options
option (vcdiff_build_exec "Build vcdiff executable" OFF)
option (vcdiff_use_system_gflags "Use system wide installed gflags" OFF)
option (vcdiff_use_system_gtest "Use system wide installed gtest" OFF)
option (BUILD_SHARED_LIBS "Request build of shared libraries." OFF)
option (BUILD_STATIC_LIBS "Request build of static libraries (default if BUILD_SHARED_LIBS is OFF)." ON)
option (BUILD_TESTING "Enable build of the unit tests and their execution using CTest." OFF)

if (NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
  set (BUILD_STATIC_LIBS ON)
endif ()

# Add subprojects first so we won't interfere with config.h.in

# Copy value. We'll override BUILD_TESTING for subprojects
set (vcdiff_build_tests ${BUILD_TESTING})

if (vcdiff_build_exec)
  set (GFLAGS_BUILD_gflags_LIB OFF)
  set (GFLAGS_BUILD_gflags_nothreads_LIB ON)
  set (GFLAGS_INSTALL_HEADERS OFF)
  set (GFLAGS_INSTALL_SHARED_LIBS OFF)
  set (GFLAGS_INSTALL_STATIC_LIBS OFF)
  set (BUILD_TESTING OFF)
  if (${vcdiff_use_system_gflags})
    find_library(gflags gflags)
  else()
    add_subdirectory (third_party/gflags)
  endif()
endif ()

include (CheckIncludeFiles)
check_include_files (ext/rope HAVE_EXT_ROPE)
check_include_files (getopt.h HAVE_GETOPT_H)
check_include_files (malloc.h HAVE_MALLOC_H)
check_include_files (sys/mman.h HAVE_SYS_MMAN_H)
check_include_files (sys/time.h HAVE_SYS_TIME_H)
check_include_files (sys/stat.h HAVE_SYS_STAT_H)
check_include_files (unistd.h HAVE_UNISTD_H)
check_include_files (windows.h HAVE_WINDOWS_H)

include (CheckFunctionExists)
check_function_exists (gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists (memalign HAVE_MEMALIGN)
check_function_exists (posix_memalign HAVE_POSIX_MEMALIGN)
check_function_exists (mprotect HAVE_MPROTECT)

configure_file (${PROJECT_SOURCE_DIR}/src/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories ("${PROJECT_BINARY_DIR}")

include_directories (${PROJECT_SOURCE_DIR}/src)
include_directories (${PROJECT_SOURCE_DIR}/src/zlib)

set (VCDCOM_SRC
  "src/google/format_extension_flags.h"
  "src/google/output_string.h"
  "src/addrcache.h"
  "src/checksum.h"
  "src/codetable.h"
  "src/logging.h"
  "src/unique_ptr.h"
  "src/varint_bigendian.h"
  "src/vcdiff_defs.h"
  "src/zlib/zlib_old.h"
  "src/zlib/zconf.h"
  "src/zlib/adler32.c"
  "src/addrcache.cc"
  "src/codetable.cc"
  "src/logging.cc"
  "src/varint_bigendian.cc"
)

set (VCDDEC_SRC
  "src/decodetable.cc"
  "src/headerparser.cc"
  "src/vcdecoder.cc"
)

set (VCDENC_SRC
  "src/blockhash.cc"
  "src/encodetable.cc"
  "src/instruction_map.cc"
  "src/jsonwriter.cc"
  "src/vcdiffengine.cc"
  "src/vcencoder.cc"
)

foreach (TYPE IN ITEMS STATIC SHARED)
  if (BUILD_${TYPE}_LIBS)
    add_library (vcdcom_${TYPE} ${TYPE} ${VCDCOM_SRC})
    set_target_properties (vcdcom_${TYPE} PROPERTIES
      OUTPUT_NAME vcdcom
      VERSION ${OPEN_VCDIFF_VERSION}
      SOVERSION ${PROJECT_SOVERSION})

    add_library (vcddec_${TYPE} ${TYPE} ${VCDDEC_SRC})
    set_target_properties (vcddec_${TYPE} PROPERTIES
      OUTPUT_NAME vcddec
      VERSION ${OPEN_VCDIFF_VERSION}
      SOVERSION ${PROJECT_SOVERSION})
    target_link_libraries (vcddec_${TYPE} vcdcom_${TYPE})

    add_library (vcdenc_${TYPE} ${TYPE} ${VCDENC_SRC})
    set_target_properties (vcdenc_${TYPE} PROPERTIES
      OUTPUT_NAME vcdenc
      VERSION ${OPEN_VCDIFF_VERSION}
      SOVERSION ${PROJECT_SOVERSION})
    target_link_libraries (vcdenc_${TYPE} vcdcom_${TYPE})

    install (TARGETS vcdcom_${TYPE} vcdenc_${TYPE} vcddec_${TYPE} DESTINATION lib)

  endif ()  # BUILD_${TYPE}_LIBS
endforeach ()

install (DIRECTORY src/google DESTINATION include)

# Setup aliases for next executable targets
if (BUILD_STATIC_LIBS)
  add_library (vcdcom ALIAS vcdcom_STATIC)
  add_library (vcddec ALIAS vcddec_STATIC)
  add_library (vcdenc ALIAS vcdenc_STATIC)
  target_compile_options(vcdcom_STATIC PRIVATE -w)
  target_compile_options(vcddec_STATIC PRIVATE -w)
  target_compile_options(vcdenc_STATIC PRIVATE -w)
else ()
  add_library (vcdcom ALIAS vcdcom_SHARED)
  add_library (vcddec ALIAS vcddec_SHARED)
  add_library (vcdenc ALIAS vcdenc_SHARED)
endif()



if (vcdiff_build_exec)
  add_executable (vcdiff "src/vcdiff_main.cc")
  target_link_libraries (vcdiff vcddec vcdenc gflags)
  install (TARGETS vcdiff DESTINATION bin)
  install (FILES man/vcdiff.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
endif ()

if (vcdiff_build_tests)
  enable_testing ()
  if (${vcdiff_use_system_gtest})
    find_package (GTest)
    if (${GTEST_FOUND})
      set (gtest_main ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBARY})
      include_directories(${GTEST_INCLUDE_DIRS})
    else()
      # On Debian it doens't install libgtest.a. Instead it provides sources in
      # /usr/src/gtest
      add_subdirectory (/usr/src/gtest gtest)
    endif()
  else()
    set (gtest_install OFF CACHE BOOL "Don't install gtest" FORCE)
    add_subdirectory (third_party/googletest/googletest)
  endif()

  add_executable (addrcache_test src/addrcache_test.cc)
  target_link_libraries (addrcache_test vcdcom gtest_main)
  add_test (addrcache_test addrcache_test)

  add_executable (blockhash_test src/blockhash_test.cc)
  target_link_libraries (blockhash_test vcdenc gtest_main)
  add_test (blockhash_test blockhash_test)

  add_executable (codetable_test src/codetable_test.cc)
  target_link_libraries (codetable_test vcdcom gtest_main)
  add_test (codetable_test codetable_test)

  add_executable (decodetable_test src/decodetable_test.cc)
  target_link_libraries (decodetable_test vcddec vcdcom gtest_main)
  add_test (decodetable_test decodetable_test)

  add_executable (encodetable_test src/encodetable_test.cc)
  target_link_libraries (encodetable_test vcdenc vcdcom gtest_main)
  add_test (encodetable_test encodetable_test)

  add_executable (headerparser_test src/headerparser_test.cc)
  target_link_libraries (headerparser_test vcddec vcdcom gtest_main)
  add_test (headerparser_test headerparser_test)

  add_executable (instruction_map_test src/instruction_map_test.cc)
  target_link_libraries (instruction_map_test vcdenc vcdcom gtest_main)
  add_test (instruction_map_test instruction_map_test)

  add_executable (output_string_test src/output_string_test.cc)
  target_link_libraries (output_string_test gtest_main)
  add_test (output_string_test output_string_test)

  add_executable (rolling_hash_test src/rolling_hash_test.cc)
  target_link_libraries (rolling_hash_test vcdcom gtest_main)
  add_test (rolling_hash_test rolling_hash_test)

  add_executable (varint_bigendian_test src/varint_bigendian_test.cc)
  target_link_libraries (varint_bigendian_test vcdcom gtest_main)
  add_test (varint_bigendian_test varint_bigendian_test)

  add_executable (vcdecoder1_test src/vcdecoder1_test.cc src/vcdecoder_test.cc)
  target_link_libraries (vcdecoder1_test vcddec gtest_main)
  add_test (vcdecoder1_test vcdecoder1_test)

  add_executable (vcdecoder2_test src/vcdecoder2_test.cc src/vcdecoder_test.cc)
  target_link_libraries (vcdecoder2_test vcddec gtest_main)
  add_test (vcdecoder2_test vcdecoder2_test)

  add_executable (vcdecoder3_test src/vcdecoder3_test.cc src/vcdecoder_test.cc)
  target_link_libraries (vcdecoder3_test vcddec gtest_main)
  add_test (vcdecoder3_test vcdecoder3_test)

  add_executable (vcdecoder4_test src/vcdecoder4_test.cc src/vcdecoder_test.cc)
  target_link_libraries (vcdecoder4_test vcddec gtest_main)
  add_test (vcdecoder4_test vcdecoder4_test)

  add_executable (vcdecoder5_test src/vcdecoder5_test.cc src/vcdecoder_test.cc)
  target_link_libraries (vcdecoder5_test vcddec gtest_main)
  add_test (vcdecoder5_test vcdecoder5_test)

  add_executable (vcdiffengine_test src/vcdiffengine_test.cc)
  target_link_libraries (vcdiffengine_test vcdenc vcdcom gtest_main)
  add_test (vcdiffengine_test vcdiffengine_test)

  add_executable (vcencoder_test src/vcencoder_test.cc)
  target_link_libraries (vcencoder_test vcddec vcdenc vcdcom gtest_main)
  add_test (vcencoder_test vcencoder_test)

  add_executable (jsonwriter_test src/jsonwriter_test.cc)
  target_link_libraries (jsonwriter_test vcdenc vcdcom gtest_main)
  add_test (jsonwriter_test jsonwriter_test)

  if (${UNIX})
    add_test (
      NAME vcdiff_test
      COMMAND ${PROJECT_SOURCE_DIR}/src/vcdiff_test.sh ${PROJECT_SOURCE_DIR})
  elseif (${WINDOWS})
    add_test (
      NAME vcdiff_test
      COMMAND ${PROJECT_SOURCE_DIR}/src/vcdiff_test.bat vcdiff)
  endif()

endif ()  # vcdiff_build_tests
