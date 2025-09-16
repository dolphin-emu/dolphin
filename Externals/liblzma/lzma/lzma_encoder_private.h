// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       lzma_encoder_private.h
/// \brief      Private definitions for LZMA encoder
///
//  Authors:    Igor Pavlov
//              Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LZMA_LZMA_ENCODER_PRIVATE_H
#define LZMA_LZMA_ENCODER_PRIVATE_H

#include "lz_encoder.h"
#include "range_encoder.h"
#include "lzma_common.h"
#include "lzma_encoder.h"


// Macro to compare if the first two bytes in two buffers differ. This is
// needed in lzma_lzma_optimum_*() to test if the match is at least
// MATCH_LEN_MIN bytes. Unaligned access gives tiny gain so there's no
// reason to not use it when it is supported.
#ifdef TUKLIB_FAST_UNALIGNED_ACCESS
#	define not_equal_16(a, b) (read16ne(a) != read16ne(b))
#else
#	define not_equal_16(a, b) \
		((a)[0] != (b)[0] || (a)[1] != (b)[1])
#endif


// Optimal - Number of entries in the optimum array.
#define OPTS (1 << 12)


typedef struct {
	probability choice;
	probability choice2;
	probability low[POS_STATES_MAX][LEN_LOW_SYMBOLS];
	probability mid[POS_STATES_MAX][LEN_MID_SYMBOLS];
	probability high[LEN_HIGH_SYMBOLS];

	uint32_t prices[POS_STATES_MAX][LEN_SYMBOLS];
	uint32_t table_size;
	uint32_t counters[POS_STATES_MAX];

} lzma_length_encoder;


typedef struct {
	lzma_lzma_state state;

	bool prev_1_is_literal;
	bool prev_2;

	uint32_t pos_prev_2;
	uint32_t back_prev_2;

	uint32_t price;
	uint32_t pos_prev;  // pos_next;
	uint32_t back_prev;

	uint32_t backs[REPS];

} lzma_optimal;


struct lzma_lzma1_encoder_s {
	/// Range encoder
	lzma_range_encoder rc;

	/// Uncompressed size (doesn't include possible preset dictionary)
	uint64_t uncomp_size;

	/// If non-zero, produce at most this much output.
	/// Some input may then be missing from the output.
	uint64_t out_limit;

	/// If the above out_limit is non-zero, *uncomp_size_ptr is set to
	/// the amount of uncompressed data that we were able to fit
	/// in the output buffer.
	uint64_t *uncomp_size_ptr;

	/// State
	lzma_lzma_state state;

	/// The four most recent match distances
	uint32_t reps[REPS];

	/// Array of match candidates
	lzma_match matches[MATCH_LEN_MAX + 1];

	/// Number of match candidates in matches[]
	uint32_t matches_count;

	/// Variable to hold the length of the longest match between calls
	/// to lzma_lzma_optimum_*().
	uint32_t longest_match_length;

	/// True if using getoptimumfast
	bool fast_mode;

	/// True if the encoder has been initialized by encoding the first
	/// byte as a literal.
	bool is_initialized;

	/// True if the range encoder has been flushed, but not all bytes
	/// have been written to the output buffer yet.
	bool is_flushed;

	/// True if end of payload marker will be written.
	bool use_eopm;

	uint32_t pos_mask;         ///< (1 << pos_bits) - 1
	uint32_t literal_context_bits;
	uint32_t literal_mask;

	// These are the same as in lzma_decoder.c. See comments there.
	probability literal[LITERAL_CODERS_MAX * LITERAL_CODER_SIZE];
	probability is_match[STATES][POS_STATES_MAX];
	probability is_rep[STATES];
	probability is_rep0[STATES];
	probability is_rep1[STATES];
	probability is_rep2[STATES];
	probability is_rep0_long[STATES][POS_STATES_MAX];
	probability dist_slot[DIST_STATES][DIST_SLOTS];
	probability dist_special[FULL_DISTANCES - DIST_MODEL_END];
	probability dist_align[ALIGN_SIZE];

	// These are the same as in lzma_decoder.c except that the encoders
	// include also price tables.
	lzma_length_encoder match_len_encoder;
	lzma_length_encoder rep_len_encoder;

	// Price tables
	uint32_t dist_slot_prices[DIST_STATES][DIST_SLOTS];
	uint32_t dist_prices[DIST_STATES][FULL_DISTANCES];
	uint32_t dist_table_size;
	uint32_t match_price_count;

	uint32_t align_prices[ALIGN_SIZE];
	uint32_t align_price_count;

	// Optimal
	uint32_t opts_end_index;
	uint32_t opts_current_index;
	lzma_optimal opts[OPTS];
};


extern void lzma_lzma_optimum_fast(
		lzma_lzma1_encoder *restrict coder, lzma_mf *restrict mf,
		uint32_t *restrict back_res, uint32_t *restrict len_res);

extern void lzma_lzma_optimum_normal(lzma_lzma1_encoder *restrict coder,
		lzma_mf *restrict mf, uint32_t *restrict back_res,
		uint32_t *restrict len_res, uint32_t position);

#endif
