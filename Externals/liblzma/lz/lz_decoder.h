// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       lz_decoder.h
/// \brief      LZ out window
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_LZ_DECODER_H
#define LZMA_LZ_DECODER_H

#include "common.h"


/// Maximum length of a match rounded up to a nice power of 2 which is
/// a good size for aligned memcpy(). The allocated dictionary buffer will
/// be 2 * LZ_DICT_REPEAT_MAX bytes larger than the actual dictionary size:
///
/// (1) Every time the decoder reaches the end of the dictionary buffer,
///     the last LZ_DICT_REPEAT_MAX bytes will be copied to the beginning.
///     This way dict_repeat() will only need to copy from one place,
///     never from both the end and beginning of the buffer.
///
/// (2) The other LZ_DICT_REPEAT_MAX bytes is kept as a buffer between
///     the oldest byte still in the dictionary and the current write
///     position. This way dict_repeat(dict, dict->size - 1, &len)
///     won't need memmove() as the copying cannot overlap.
///
/// Note that memcpy() still cannot be used if distance < len.
///
/// LZMA's longest match length is 273 so pick a multiple of 16 above that.
#define LZ_DICT_REPEAT_MAX 288


typedef struct {
	/// Pointer to the dictionary buffer.
	uint8_t *buf;

	/// Write position in dictionary. The next byte will be written to
	/// buf[pos].
	size_t pos;

	/// Indicates how full the dictionary is. This is used by
	/// dict_is_distance_valid() to detect corrupt files that would
	/// read beyond the beginning of the dictionary.
	size_t full;

	/// Write limit
	size_t limit;

	/// Allocated size of buf. This is 2 * LZ_DICT_REPEAT_MAX bytes
	/// larger than the actual dictionary size. This is enforced by
	/// how the value for "full" is set; it can be at most
	/// "size - 2 * LZ_DICT_REPEAT_MAX".
	size_t size;

	/// True once the dictionary has become full and the writing position
	/// has been wrapped in decode_buffer() in lz_decoder.c.
	bool has_wrapped;

	/// True when dictionary should be reset before decoding more data.
	bool need_reset;

} lzma_dict;


typedef struct {
	size_t dict_size;
	const uint8_t *preset_dict;
	size_t preset_dict_size;
} lzma_lz_options;


typedef struct {
	/// Data specific to the LZ-based decoder
	void *coder;

	/// Function to decode from in[] to *dict
	lzma_ret (*code)(void *coder,
			lzma_dict *restrict dict, const uint8_t *restrict in,
			size_t *restrict in_pos, size_t in_size);

	void (*reset)(void *coder, const void *options);

	/// Set the uncompressed size. If uncompressed_size == LZMA_VLI_UNKNOWN
	/// then allow_eopm will always be true.
	void (*set_uncompressed)(void *coder, lzma_vli uncompressed_size,
			bool allow_eopm);

	/// Free allocated resources
	void (*end)(void *coder, const lzma_allocator *allocator);

} lzma_lz_decoder;


#define LZMA_LZ_DECODER_INIT \
	(lzma_lz_decoder){ \
		.coder = NULL, \
		.code = NULL, \
		.reset = NULL, \
		.set_uncompressed = NULL, \
		.end = NULL, \
	}


extern lzma_ret lzma_lz_decoder_init(lzma_next_coder *next,
		const lzma_allocator *allocator,
		const lzma_filter_info *filters,
		lzma_ret (*lz_init)(lzma_lz_decoder *lz,
			const lzma_allocator *allocator,
			lzma_vli id, const void *options,
			lzma_lz_options *lz_options));

extern uint64_t lzma_lz_decoder_memusage(size_t dictionary_size);


//////////////////////
// Inline functions //
//////////////////////

/// Get a byte from the history buffer.
static inline uint8_t
dict_get(const lzma_dict *const dict, const uint32_t distance)
{
	return dict->buf[dict->pos - distance - 1
			+ (distance < dict->pos
				? 0 : dict->size - LZ_DICT_REPEAT_MAX)];
}


/// Optimized version of dict_get(dict, 0)
static inline uint8_t
dict_get0(const lzma_dict *const dict)
{
	return dict->buf[dict->pos - 1];
}


/// Test if dictionary is empty.
static inline bool
dict_is_empty(const lzma_dict *const dict)
{
	return dict->full == 0;
}


/// Validate the match distance
static inline bool
dict_is_distance_valid(const lzma_dict *const dict, const size_t distance)
{
	return dict->full > distance;
}


/// Repeat *len bytes at distance.
static inline bool
dict_repeat(lzma_dict *dict, uint32_t distance, uint32_t *len)
{
	// Don't write past the end of the dictionary.
	const size_t dict_avail = dict->limit - dict->pos;
	uint32_t left = my_min(dict_avail, *len);
	*len -= left;

	size_t back = dict->pos - distance - 1;
	if (distance >= dict->pos)
		back += dict->size - LZ_DICT_REPEAT_MAX;

	// Repeat a block of data from the history. Because memcpy() is faster
	// than copying byte by byte in a loop, the copying process gets split
	// into two cases.
	if (distance < left) {
		// Source and target areas overlap, thus we can't use
		// memcpy() nor even memmove() safely.
		do {
			dict->buf[dict->pos++] = dict->buf[back++];
		} while (--left > 0);
	} else {
		memcpy(dict->buf + dict->pos, dict->buf + back, left);
		dict->pos += left;
	}

	// Update how full the dictionary is.
	if (!dict->has_wrapped)
		dict->full = dict->pos - 2 * LZ_DICT_REPEAT_MAX;

	return *len != 0;
}


static inline void
dict_put(lzma_dict *dict, uint8_t byte)
{
	dict->buf[dict->pos++] = byte;

	if (!dict->has_wrapped)
		dict->full = dict->pos - 2 * LZ_DICT_REPEAT_MAX;
}


/// Puts one byte into the dictionary. Returns true if the dictionary was
/// already full and the byte couldn't be added.
static inline bool
dict_put_safe(lzma_dict *dict, uint8_t byte)
{
	if (unlikely(dict->pos == dict->limit))
		return true;

	dict_put(dict, byte);
	return false;
}


/// Copies arbitrary amount of data into the dictionary.
static inline void
dict_write(lzma_dict *restrict dict, const uint8_t *restrict in,
		size_t *restrict in_pos, size_t in_size,
		size_t *restrict left)
{
	// NOTE: If we are being given more data than the size of the
	// dictionary, it could be possible to optimize the LZ decoder
	// so that not everything needs to go through the dictionary.
	// This shouldn't be very common thing in practice though, and
	// the slowdown of one extra memcpy() isn't bad compared to how
	// much time it would have taken if the data were compressed.

	if (in_size - *in_pos > *left)
		in_size = *in_pos + *left;

	*left -= lzma_bufcpy(in, in_pos, in_size,
			dict->buf, &dict->pos, dict->limit);

	if (!dict->has_wrapped)
		dict->full = dict->pos - 2 * LZ_DICT_REPEAT_MAX;

	return;
}


static inline void
dict_reset(lzma_dict *dict)
{
	dict->need_reset = true;
	return;
}

#endif
