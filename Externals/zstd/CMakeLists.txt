project(zstd C ASM)

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
    zstd/lib/zstd.h
    zstd/lib/zstd_errors.h
)
set(ZSTD_PRIVATE_HDRS
    zstd/lib/common/allocations.h
    zstd/lib/common/bits.h
    zstd/lib/common/bitstream.h
    zstd/lib/common/compiler.h
    zstd/lib/common/cpu.h
    zstd/lib/common/debug.h
    zstd/lib/common/error_private.h
    zstd/lib/common/fse.h
    zstd/lib/common/huf.h
    zstd/lib/common/mem.h
    zstd/lib/common/pool.h
    zstd/lib/common/portability_macros.h
    zstd/lib/common/threading.h
    zstd/lib/common/xxhash.h
    zstd/lib/common/zstd_deps.h
    zstd/lib/common/zstd_internal.h
    zstd/lib/common/zstd_trace.h
    zstd/lib/compress/clevels.h
    zstd/lib/compress/hist.h
    zstd/lib/compress/zstd_compress_internal.h
    zstd/lib/compress/zstd_compress_literals.h
    zstd/lib/compress/zstd_compress_sequences.h
    zstd/lib/compress/zstd_compress_superblock.h
    zstd/lib/compress/zstd_cwksp.h
    zstd/lib/compress/zstd_double_fast.h
    zstd/lib/compress/zstd_fast.h
    zstd/lib/compress/zstd_lazy.h
    zstd/lib/compress/zstd_ldm.h
    zstd/lib/compress/zstd_ldm_geartab.h
    zstd/lib/compress/zstd_opt.h
    zstd/lib/compress/zstd_preSplit.h
    zstd/lib/compress/zstdmt_compress.h
    zstd/lib/decompress/zstd_ddict.h
    zstd/lib/decompress/zstd_decompress_block.h
    zstd/lib/decompress/zstd_decompress_internal.h
)
set(ZSTD_SRCS
    zstd/lib/common/debug.c
    zstd/lib/common/entropy_common.c
    zstd/lib/common/error_private.c
    zstd/lib/common/fse_decompress.c
    zstd/lib/common/pool.c
    zstd/lib/common/threading.c
    zstd/lib/common/xxhash.c
    zstd/lib/common/zstd_common.c
    zstd/lib/compress/fse_compress.c
    zstd/lib/compress/hist.c
    zstd/lib/compress/huf_compress.c
    zstd/lib/compress/zstd_compress.c
    zstd/lib/compress/zstd_compress_literals.c
    zstd/lib/compress/zstd_compress_sequences.c
    zstd/lib/compress/zstd_compress_superblock.c
    zstd/lib/compress/zstd_double_fast.c
    zstd/lib/compress/zstd_fast.c
    zstd/lib/compress/zstd_lazy.c
    zstd/lib/compress/zstd_ldm.c
    zstd/lib/compress/zstd_opt.c
    zstd/lib/compress/zstd_preSplit.c
    zstd/lib/compress/zstdmt_compress.c
    zstd/lib/decompress/huf_decompress.c
    zstd/lib/decompress/huf_decompress_amd64.S
    zstd/lib/decompress/zstd_ddict.c
    zstd/lib/decompress/zstd_decompress.c
    zstd/lib/decompress/zstd_decompress_block.c
)

add_library(zstd STATIC ${ZSTD_SRCS} ${ZSTD_PUBLIC_HDRS} ${ZSTD_PRIVATE_HDRS})
target_compile_definitions(zstd PUBLIC ZSTD_LEGACY_SUPPORT=0)

dolphin_disable_warnings(zstd)
add_library(zstd::zstd ALIAS zstd)

target_include_directories(zstd
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/zstd/lib
)
