// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc32_small.c
/// \brief      CRC32 calculation (size-optimized)
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"
#include "crc_common.h"


// The table is used by the LZ encoder too, thus it's not static like
// in crc64_small.c.
uint32_t lzma_crc32_table[1][256];


#ifdef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
__attribute__((__constructor__))
#endif
static void
crc32_init(void)
{
	static const uint32_t poly32 = UINT32_C(0xEDB88320);

	for (size_t b = 0; b < 256; ++b) {
		uint32_t r = b;
		for (size_t i = 0; i < 8; ++i) {
			if (r & 1)
				r = (r >> 1) ^ poly32;
			else
				r >>= 1;
		}

		lzma_crc32_table[0][b] = r;
	}

	return;
}


#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
extern void
lzma_crc32_init(void)
{
	mythread_once(crc32_init);
	return;
}
#endif


extern LZMA_API(uint32_t)
lzma_crc32(const uint8_t *buf, size_t size, uint32_t crc)
{
#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
	lzma_crc32_init();
#endif

	crc = ~crc;

	while (size != 0) {
		crc = lzma_crc32_table[0][*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
		--size;
	}

	return ~crc;
}
