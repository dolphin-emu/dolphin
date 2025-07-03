// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc64_table.c
/// \brief      Precalculated CRC64 table with correct endianness
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


// FIXME: Compared to crc_common.h this has to check for __x86_64__ too
// so that in 32-bit builds crc64_x86.S won't break due to a missing table.
#if defined(HAVE_USABLE_CLMUL) && ((defined(__x86_64__) && defined(__SSSE3__) \
			&& defined(__SSE4_1__) && defined(__PCLMUL__)) \
		|| (defined(__e2k__) && __iset__ >= 6))
#	define NO_CRC64_TABLE
#endif


#ifdef NO_CRC64_TABLE
// No table needed. Use a typedef to avoid an empty translation unit.
typedef void lzma_crc64_dummy;

#else
// Having the declaration here silences clang -Wmissing-variable-declarations.
extern const uint64_t lzma_crc64_table[4][256];

#	if defined(WORDS_BIGENDIAN)
#		include "crc64_table_be.h"
#	else
#		include "crc64_table_le.h"
#	endif
#endif
