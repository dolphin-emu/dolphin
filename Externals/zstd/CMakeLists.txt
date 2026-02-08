project(zstd C)

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
   add_definitions(-D_LARGEFILE64_SOURCE=1)
endif()
set(CMAKE_REQUIRED_DEFINITIONS) # clear variable

# Check for fseeko
check_function_exists(fseeko HAVE_FSEEKO)
if(NOT HAVE_FSEEKO)
    add_definitions(-DNO_FSEEKO)
endif()

#
# Check for unistd.h
#
check_include_file(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
    add_definitions(-DHAVE_UNISTD_H)
endif()

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

# Always hide XXHash symbols
add_definitions(-DXXH_NAMESPACE=ZSTD_)

#============================================================================
# zstd
#============================================================================

set(ZSTD_PUBLIC_HDRS
    lib/zstd.h
)
set(ZSTD_PRIVATE_HDRS
    lib/common/bitstream.h
    lib/common/compiler.h
    lib/common/cpu.h
    lib/common/debug.h
    lib/common/error_private.h
    lib/common/fse.h
    lib/common/huf.h
    lib/common/mem.h
    lib/common/pool.h
    lib/common/threading.h
    lib/common/xxhash.h
    lib/common/zstd_errors.h
    lib/common/zstd_internal.h
    lib/compress/hist.h
    lib/compress/zstd_compress_internal.h
    lib/compress/zstd_compress_literals.h
    lib/compress/zstd_compress_sequences.h
    lib/compress/zstd_cwksp.h
    lib/compress/zstd_double_fast.h
    lib/compress/zstd_fast.h
    lib/compress/zstd_lazy.h
    lib/compress/zstd_ldm.h
    lib/compress/zstd_opt.h
    lib/compress/zstdmt_compress.h
    lib/decompress/zstd_ddict.h
    lib/decompress/zstd_decompress_block.h
    lib/decompress/zstd_decompress_internal.h
)
set(ZSTD_SRCS
    lib/common/debug.c
    lib/common/entropy_common.c
    lib/common/error_private.c
    lib/common/fse_decompress.c
    lib/common/pool.c
    lib/common/threading.c
    lib/common/xxhash.c
    lib/common/zstd_common.c
    lib/compress/fse_compress.c
    lib/compress/hist.c
    lib/compress/huf_compress.c
    lib/compress/zstd_compress.c
    lib/compress/zstd_compress_literals.c
    lib/compress/zstd_compress_sequences.c
    lib/compress/zstd_double_fast.c
    lib/compress/zstd_fast.c
    lib/compress/zstd_lazy.c
    lib/compress/zstd_ldm.c
    lib/compress/zstd_opt.c
    lib/compress/zstdmt_compress.c
    lib/decompress/huf_decompress.c
    lib/decompress/zstd_ddict.c
    lib/decompress/zstd_decompress.c
    lib/decompress/zstd_decompress_block.c
)

add_library(zstd STATIC ${ZSTD_SRCS} ${ZSTD_PUBLIC_HDRS} ${ZSTD_PRIVATE_HDRS})
dolphin_disable_warnings(zstd)
add_library(zstd::zstd ALIAS zstd)

target_include_directories(zstd
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/compress
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/decompress
)
