// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _AUDIO_RESAMPLER_H_
#define _AUDIO_RESAMPLER_H_

#include <functional>

#include "Common.h"

namespace Common
{

enum ResamplingType
{
	// No resampling
	RESAMPLING_NONE,

	// Resampling using a linear interpolation
	RESAMPLING_LINEAR,

	// Resampling using a FIR/polyphase filter. Coefficients for the filter
	// have to be provided.
	RESAMPLING_POLYPHASE,
};

// Reads samples from the input callback, resamples them to <count> samples at
// the wanted sample rate (computed from the ratio, see below).
//
// If srctype is RESAMPLING_POLYPHASE, coefficients need to be provided as well
// (or the srctype will automatically be changed to LINEAR).
//
// Returns the current position after resampling (including fractional part).
//
// The stride represents "how much is the output pointer incremented after each
// sample write". In most cases, you want 1 (uninterrupted output array).
//
// The input to output ratio is set in <ratio>, which is a floating point num
// stored as a 32b integer:
//  * Upper 16 bits of the ratio are the integer part
//  * Lower 16 bits are the decimal part
//
// <curr_pos> is a 32b integer structured in the same way as the ratio: the
// upper 16 bits are the integer part of the current position in the input
// stream, and the lower 16 bits are the decimal part.
//
// We start getting samples not from sample 0, but 0.<curr_pos_frac>. This
// avoids discontinuities in the audio stream, especially with very low ratios
// which interpolate a lot of values between two "real" samples.
u32 ResampleAudio(std::function<s16(u32)> input_callback, s16* output, u32 count,
                  u32 stride, s16* last_samples, u32 curr_pos, u32 ratio,
                  ResamplingType srctype, const s16* coeffs);

// Compute the number of samples that will need to be fetched from the input
// callback, assuming we are at position <curr_pos> and we want to get <count>
// samples at a given ratio <ratio>, using the <srctype> interpolation method.
u32 GetCountNeededForResample(u32 count, u32 curr_pos, u32 ratio,
                              ResamplingType srctype);

}

#endif
