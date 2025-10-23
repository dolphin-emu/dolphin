// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       powerpc.c
/// \brief      Filter for PowerPC (big endian) binaries
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"


static size_t
powerpc_code(void *simple lzma_attribute((__unused__)),
		uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	size &= ~(size_t)3;

	size_t i;
	for (i = 0; i < size; i += 4) {
		// PowerPC branch 6(48) 24(Offset) 1(Abs) 1(Link)
		if ((buffer[i] >> 2) == 0x12
				&& ((buffer[i + 3] & 3) == 1)) {

			const uint32_t src
				= (((uint32_t)(buffer[i + 0]) & 3) << 24)
				| ((uint32_t)(buffer[i + 1]) << 16)
				| ((uint32_t)(buffer[i + 2]) << 8)
				| ((uint32_t)(buffer[i + 3]) & ~UINT32_C(3));

			uint32_t dest;
			if (is_encoder)
				dest = now_pos + (uint32_t)(i) + src;
			else
				dest = src - (now_pos + (uint32_t)(i));

			buffer[i + 0] = 0x48 | ((dest >> 24) &  0x03);
			buffer[i + 1] = (dest >> 16);
			buffer[i + 2] = (dest >> 8);
			buffer[i + 3] &= 0x03;
			buffer[i + 3] |= dest;
		}
	}

	return i;
}


static lzma_ret
powerpc_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	return lzma_simple_coder_init(next, allocator, filters,
			&powerpc_code, 0, 4, 4, is_encoder);
}


#ifdef HAVE_ENCODER_POWERPC
extern lzma_ret
lzma_simple_powerpc_encoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return powerpc_coder_init(next, allocator, filters, true);
}
#endif


#ifdef HAVE_DECODER_POWERPC
extern lzma_ret
lzma_simple_powerpc_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return powerpc_coder_init(next, allocator, filters, false);
}
#endif
