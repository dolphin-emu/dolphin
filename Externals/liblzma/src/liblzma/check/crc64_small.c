// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       crc64_small.c
/// \brief      CRC64 calculation (size-optimized)
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "check.h"


static uint64_t crc64_table[256];


#ifdef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
__attribute__((__constructor__))
#endif
static void
crc64_init(void)
{
	static const uint64_t poly64 = UINT64_C(0xC96C5795D7870F42);

	for (size_t b = 0; b < 256; ++b) {
		uint64_t r = b;
		for (size_t i = 0; i < 8; ++i) {
			if (r & 1)
				r = (r >> 1) ^ poly64;
			else
				r >>= 1;
		}

		crc64_table[b] = r;
	}

	return;
}


extern LZMA_API(uint64_t)
lzma_crc64(const uint8_t *buf, size_t size, uint64_t crc)
{
#ifndef HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR
	mythread_once(crc64_init);
#endif

	crc = ~crc;

	while (size != 0) {
		crc = crc64_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
		--size;
	}

	return ~crc;
}
