// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_table.c
/// \brief      Precalculated CRC32 table with correct endianness
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


// FIXME: Compared to crc_common.h this has to check for __x86_64__ too
// so that in 32-bit builds crc32_x86.S won't break due to a missing table.
#if defined(HAVE_USABLE_CLMUL) && ((defined(__x86_64__) && defined(__SSSE3__) \
			&& defined(__SSE4_1__) && defined(__PCLMUL__)) \
		|| (defined(__e2k__) && __iset__ >= 6))
#	define NO_CRC32_TABLE

#elif defined(HAVE_ARM64_CRC32) \
		&& !defined(WORDS_BIGENDIAN) \
		&& defined(__ARM_FEATURE_CRC32)
#	define NO_CRC32_TABLE
#endif


#if !defined(HAVE_ENCODERS) && defined(NO_CRC32_TABLE)
// No table needed. Use a typedef to avoid an empty translation unit.
typedef void lzma_crc32_dummy;

#else
// Having the declaration here silences clang -Wmissing-variable-declarations.
extern const uint32_t lzma_crc32_table[8][256];

#	ifdef WORDS_BIGENDIAN
#		include "crc32_table_be.h"
#	else
#		include "crc32_table_le.h"
#	endif
#endif
