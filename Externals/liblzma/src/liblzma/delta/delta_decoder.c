// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       delta_decoder.c
/// \brief      Delta filter decoder
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "delta_decoder.h"
#include "delta_private.h"


static void
decode_buffer(lzma_delta_coder *coder, uint8_t *buffer, size_t size)
{
	const size_t distance = coder->distance;

	for (size_t i = 0; i < size; ++i) {
		buffer[i] += coder->history[(distance + coder->pos) & 0xFF];
		coder->history[coder->pos-- & 0xFF] = buffer[i];
	}
}


// For an unknown reason NVIDIA HPC Compiler needs this pragma
// to produce working code.
#ifdef __NVCOMPILER
#	pragma routine novector
#endif
static lzma_ret
delta_decode(void *coder_ptr, const lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size, uint8_t *restrict out,
		size_t *restrict out_pos, size_t out_size, lzma_action action)
{
	lzma_delta_coder *coder = coder_ptr;

	assert(coder->next.code != NULL);

	const size_t out_start = *out_pos;

	const lzma_ret ret = coder->next.code(coder->next.coder, allocator,
			in, in_pos, in_size, out, out_pos, out_size,
			action);

	// out might be NULL. In that case size == 0. Null pointer + 0 is
	// undefined behavior so skip the call in that case as it would
	// do nothing anyway.
	const size_t size = *out_pos - out_start;
	if (size > 0)
		decode_buffer(coder, out + out_start, size);

	return ret;
}


extern lzma_ret
lzma_delta_decoder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		const lzma_filter_info *filters)
{
	next->code = &delta_decode;
	return lzma_delta_coder_init(next, allocator, filters);
}


extern lzma_ret
lzma_delta_props_decode(void **options, const lzma_allocator *allocator,
		const uint8_t *props, size_t props_size)
{
	if (props_size != 1)
		return LZMA_OPTIONS_ERROR;

	lzma_options_delta *opt
			= lzma_alloc(sizeof(lzma_options_delta), allocator);
	if (opt == NULL)
		return LZMA_MEM_ERROR;

	opt->type = LZMA_DELTA_TYPE_BYTE;
	opt->dist = props[0] + 1U;

	*options = opt;

	return LZMA_OK;
}
