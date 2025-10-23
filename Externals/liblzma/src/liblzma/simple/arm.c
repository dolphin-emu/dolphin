// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       arm.c
/// \brief      Filter for ARM binaries
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "simple_private.h"


static size_t
arm_code(void *simple lzma_attribute((__unused__)),
		uint32_t now_pos, bool is_encoder,
		uint8_t *buffer, size_t size)
{
	size &= ~(size_t)3;

	size_t i;
	for (i = 0; i < size; i += 4) {
		if (buffer[i + 3] == 0xEB) {
			uint32_t src = ((uint32_t)(buffer[i + 2]) << 16)
					| ((uint32_t)(buffer[i + 1]) << 8)
					| (uint32_t)(buffer[i + 0]);
			src <<= 2;

			uint32_t dest;
			if (is_encoder)
				dest = now_pos + (uint32_t)(i) + 8 + src;
			else
				dest = src - (now_pos + (uint32_t)(i) + 8);

			dest >>= 2;
			buffer[i + 2] = (dest >> 16);
			buffer[i + 1] = (dest >> 8);
			buffer[i + 0] = dest;
		}
	}

	return i;
}


static lzma_ret
arm_coder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters, bool is_encoder)
{
	return lzma_simple_coder_init(next, allocator, filters,
			&arm_code, 0, 4, 4, is_encoder);
}


#ifdef HAVE_ENCODER_ARM
extern lzma_ret
lzma_simple_arm_encoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return arm_coder_init(next, allocator, filters, true);
}
#endif


#ifdef HAVE_DECODER_ARM
extern lzma_ret
lzma_simple_arm_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	return arm_coder_init(next, allocator, filters, false);
}
#endif
