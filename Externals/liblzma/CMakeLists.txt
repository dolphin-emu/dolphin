project(lzma C)

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

add_definitions(-DHAVE_CONFIG_H)
add_definitions(-DLZMA_API_STATIC)

#============================================================================
# lzma
#============================================================================

set(LZMA_PUBLIC_HDRS
    api/lzma.h
    api/lzma/base.h
    api/lzma/bcj.h
    api/lzma/block.h
    api/lzma/check.h
    api/lzma/container.h
    api/lzma/delta.h
    api/lzma/filter.h
    api/lzma/hardware.h
    api/lzma/index.h
    api/lzma/index_hash.h
    api/lzma/lzma12.h
    api/lzma/stream_flags.h
    api/lzma/version.h
    api/lzma/vli.h
)

set(LZMA_SRCS
    check/check.c
    check/check.h
    check/crc32_fast.c
    check/crc32_table_be.h
    check/crc32_table.c
    check/crc32_table_le.h
    check/crc64_fast.c
    check/crc64_table_be.h
    check/crc64_table.c
    check/crc64_table_le.h
    check/crc_macros.h
    check/sha256.c
    common/alone_decoder.c
    common/alone_decoder.h
    common/alone_encoder.c
    common/auto_decoder.c
    common/block_buffer_decoder.c
    common/block_buffer_encoder.c
    common/block_buffer_encoder.h
    common/block_decoder.c
    common/block_decoder.h
    common/block_encoder.c
    common/block_encoder.h
    common/block_header_decoder.c
    common/block_header_encoder.c
    common/block_util.c
    common/common.c
    common/common.h
    common/easy_buffer_encoder.c
    common/easy_decoder_memusage.c
    common/easy_encoder.c
    common/easy_encoder_memusage.c
    common/easy_preset.c
    common/easy_preset.h
    common/filter_buffer_decoder.c
    common/filter_buffer_encoder.c
    common/filter_common.c
    common/filter_common.h
    common/filter_decoder.c
    common/filter_decoder.h
    common/filter_encoder.c
    common/filter_encoder.h
    common/filter_flags_decoder.c
    common/filter_flags_encoder.c
    common/hardware_cputhreads.c
    common/hardware_physmem.c
    common/index.c
    common/index_decoder.c
    common/index_encoder.c
    common/index_encoder.h
    common/index.h
    common/index_hash.c
    common/memcmplen.h
    common/outqueue.c
    common/outqueue.h
    common/stream_buffer_decoder.c
    common/stream_buffer_encoder.c
    common/stream_decoder.c
    common/stream_decoder.h
    common/stream_encoder.c
    common/stream_encoder_mt.c
    common/stream_flags_common.c
    common/stream_flags_common.h
    common/stream_flags_decoder.c
    common/stream_flags_encoder.c
    common/vli_decoder.c
    common/vli_encoder.c
    common/vli_size.c
    delta/delta_common.c
    delta/delta_common.h
    delta/delta_decoder.c
    delta/delta_decoder.h
    delta/delta_encoder.c
    delta/delta_encoder.h
    delta/delta_private.h
    lz/lz_decoder.c
    lz/lz_decoder.h
    lz/lz_encoder.c
    lz/lz_encoder.h
    lz/lz_encoder_hash.h
    lz/lz_encoder_hash_table.h
    lz/lz_encoder_mf.c
    lzma/fastpos.h
    lzma/fastpos_table.c
    lzma/lzma2_decoder.c
    lzma/lzma2_decoder.h
    lzma/lzma2_encoder.c
    lzma/lzma2_encoder.h
    lzma/lzma_common.h
    lzma/lzma_decoder.c
    lzma/lzma_decoder.h
    lzma/lzma_encoder.c
    lzma/lzma_encoder.h
    lzma/lzma_encoder_optimum_fast.c
    lzma/lzma_encoder_optimum_normal.c
    lzma/lzma_encoder_presets.c
    lzma/lzma_encoder_private.h
    rangecoder/price.h
    rangecoder/price_table.c
    rangecoder/range_common.h
    rangecoder/range_decoder.h
    rangecoder/range_encoder.h
    simple/simple_coder.c
    simple/simple_coder.h
    simple/simple_decoder.c
    simple/simple_decoder.h
    simple/simple_encoder.c
    simple/simple_encoder.h
    simple/simple_private.h
    tuklib/mythread.h
    tuklib/sysdefs.h
    tuklib/tuklib_common.h
    tuklib/tuklib_config.h
    tuklib/tuklib_cpucores.c
    tuklib/tuklib_cpucores.h
    tuklib/tuklib_exit.c
    tuklib/tuklib_exit.h
    tuklib/tuklib_gettext.h
    tuklib/tuklib_integer.h
    tuklib/tuklib_mbstr_fw.c
    tuklib/tuklib_mbstr.h
    tuklib/tuklib_mbstr_width.c
    tuklib/tuklib_open_stdxxx.c
    tuklib/tuklib_open_stdxxx.h
    tuklib/tuklib_physmem.c
    tuklib/tuklib_physmem.h
    tuklib/tuklib_progname.c
    tuklib/tuklib_progname.h
)

add_library(lzma STATIC ${LZMA_SRCS} ${LZMA_PUBLIC_HDRS})
add_library(LibLZMA::LibLZMA ALIAS lzma)
dolphin_disable_warnings(lzma)

target_compile_definitions(lzma PUBLIC LZMA_API_STATIC)

target_include_directories(lzma
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/api
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/check
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/delta
    ${CMAKE_CURRENT_SOURCE_DIR}/lz
    ${CMAKE_CURRENT_SOURCE_DIR}/lzma
    ${CMAKE_CURRENT_SOURCE_DIR}/rangecoder
    ${CMAKE_CURRENT_SOURCE_DIR}/simple
    ${CMAKE_CURRENT_SOURCE_DIR}/tuklib
)
