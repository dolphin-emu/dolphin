project(zlib C)

include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)

check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(stdint.h    HAVE_STDINT_H)
check_include_file(stddef.h    HAVE_STDDEF_H)

# Check to see if we have large file support
set(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1)
# We add these other definitions here because CheckTypeSize.cmake
# in CMake 2.4.x does not automatically do so and we want
# compatibility with CMake 2.4.x.
if(HAVE_SYS_TYPES_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_SYS_TYPES_H)
endif()
if(HAVE_STDINT_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDINT_H)
endif()
if(HAVE_STDDEF_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDDEF_H)
endif()
check_type_size(off64_t OFF64_T)
if(HAVE_OFF64_T)
    add_compile_definitions(_LARGEFILE64_SOURCE=1)
endif()
set(CMAKE_REQUIRED_DEFINITIONS) # clear variable

# Check for fseeko
check_function_exists(fseeko HAVE_FSEEKO)
if(NOT HAVE_FSEEKO)
    add_compile_definitions(NO_FSEEKO)
endif()

#
# Check for unistd.h
#
check_include_file(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
    add_compile_definitions(HAVE_UNISTD_H)
endif()

#============================================================================
# zlib
#============================================================================

set(ZLIB_PUBLIC_HDRS
    zconf.h
    zlib.h
)
set(ZLIB_PRIVATE_HDRS
    adler32_simd.h
    contrib/optimizations/chunkcopy.h
    contrib/optimizations/inffast_chunk.h
    contrib/optimizations/insert_string.h
    cpu_features.h
    crc32_simd.h
    crc32.h
    deflate.h
    gzguts.h
    inffast.h
    inffixed.h
    inflate.h
    inftrees.h
    slide_hash_simd.h
    trees.h
    zutil.h
)
set(ZLIB_SRCS
    adler32_simd.c
    adler32.c
    compress.c
    contrib/optimizations/inffast_chunk.c
    contrib/optimizations/inflate.c
    cpu_features.c
    crc_folding.c
    crc32_simd.c
    crc32.c
    deflate.c
    gzclose.c
    gzlib.c
    gzread.c
    gzwrite.c
    infback.c
    inffast.c
    inftrees.c
    trees.c
    uncompr.c
    zutil.c
)

if(_M_X86)
    if(WIN32)
        add_compile_definitions(X86_WINDOWS)
    else()
        add_compile_definitions(X86_NOT_WINDOWS)
        set_source_files_properties(
            adler32_simd.c
            adler32.c
            PROPERTIES COMPILE_FLAGS "-mssse3")
        set_source_files_properties(
            crc32_simd.c
            crc32_simd.h
            crc_folding.c
            PROPERTIES COMPILE_FLAGS "-msse4.2 -mpclmul")
    endif()
    add_compile_definitions(ADLER32_SIMD_SSSE3)
    add_compile_definitions(CRC32_SIMD_SSE42_PCLMUL)
    add_compile_definitions(DEFLATE_SLIDE_HASH_SSE2)
    add_compile_definitions(INFLATE_CHUNK_SIMD_SSE2)
elseif(_M_ARM_64)
    if(APPLE)
        add_compile_definitions(ARMV8_OS_MACOS)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(ANDROID)
            add_compile_definitions(ARMV8_OS_ANDROID)
        else()
            add_compile_definitions(ARMV8_OS_LINUX)
        endif()
    elseif(WIN32)
        add_compile_definitions(ARMV8_OS_WINDOWS)
    endif()
    if(NOT WIN32)
        set_source_files_properties(
            adler32_simd.c
            adler32.c
            PROPERTIES COMPILE_FLAGS "-march=armv8-a+crc")
    endif()
    add_compile_definitions(CRC32_ARMV8_CRC32)
    add_compile_definitions(DEFLATE_SLIDE_HASH_NEON)
    add_compile_definitions(INFLATE_CHUNK_SIMD_NEON)
endif()

add_library(z STATIC ${ZLIB_SRCS} ${ZLIB_DLL_SRCS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
dolphin_disable_warnings_msvc(z)
add_library(ZLIB::ZLIB ALIAS z)

target_include_directories(z
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
