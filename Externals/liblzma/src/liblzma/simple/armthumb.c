// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       armthumb.c
/// \brief      Filter for ARM-Thumb binaries
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"


static size_t
armthumb_code(void *simple lzma_attribute((__unused__)),
		uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	if (size < 4)
		return 0;

	size -= 4;

	size_t i;
	for (i = 0; i <= size; i += 2) {
		if ((buffer[i + 1] & 0xF8) == 0xF0
				&& (buffer[i + 3] & 0xF8) == 0xF8) {
			uint32_t src = (((uint32_t)(buffer[i + 1]) & 7) << 19)
				| ((uint32_t)(buffer[i + 0]) << 11)
				| (((uint32_t)(buffer[i + 3]) & 7) << 8)
				| (uint32_t)(buffer[i + 2]);

			src <<= 1;

			uint32_t dest;
			if (is_encoder)
				dest = now_pos + (uint32_t)(i) + 4 + src;
			else
				dest = src - (now_pos + (uint32_t)(i) + 4);

			dest >>= 1;
			buffer[i + 1] = 0xF0 | ((dest >> 19) & 0x7);
			buffer[i + 0] = (dest >> 11);
			buffer[i + 3] = 0xF8 | ((dest >> 8) & 0x7);
			buffer[i + 2] = (dest);
			i += 2;
		}
	}

	return i;
}


static lzma_ret
armthumb_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	return lzma_simple_coder_init(next, allocator, filters,
			&armthumb_code, 0, 4, 2, is_encoder);
}


#ifdef HAVE_ENCODER_ARMTHUMB
extern lzma_ret
lzma_simple_armthumb_encoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return armthumb_coder_init(next, allocator, filters, true);
}
#endif


#ifdef HAVE_DECODER_ARMTHUMB
extern lzma_ret
lzma_simple_armthumb_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return armthumb_coder_init(next, allocator, filters, false);
}
#endif
