// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AudioResampler.h"

namespace Common
{

u32 ResampleAudio(std::function<s16(u32)> input_callback, s16* output, u32 count,
                  u32 stride, s16* last_samples, u32 curr_pos, u32 ratio,
                  ResamplingType srctype, const s16* coeffs)
{
	int read_samples_count = 0;

	// TODO(delroth): find out why the polyphase resampling algorithm causes
	// audio glitches in Wii games with non integral ratios.

	// If coefficients are provided, support polyphase resampling.
	if (0) // if (coeffs && srctype == RESAMPLING_POLYPHASE)
	{
		s16 temp[4];
		u32 idx = 0;

		temp[idx++ & 3] = last_samples[0];
		temp[idx++ & 3] = last_samples[1];
		temp[idx++ & 3] = last_samples[2];
		temp[idx++ & 3] = last_samples[3];

		for (u32 i = 0; i < count; ++i)
		{
			curr_pos += ratio;
			while (curr_pos >= 0x10000)
			{
				temp[idx++ & 3] = input_callback(read_samples_count++);
				curr_pos -= 0x10000;
			}

			u16 curr_pos_frac = ((curr_pos & 0xFFFF) >> 9) << 2;
			const s16* c = &coeffs[curr_pos_frac];

			s64 t0 = temp[idx++ & 3];
			s64 t1 = temp[idx++ & 3];
			s64 t2 = temp[idx++ & 3];
			s64 t3 = temp[idx++ & 3];

			s64 samp = (t0 * c[0] + t1 * c[1] + t2 * c[2] + t3 * c[3]) >> 15;

			*output = (s16)samp;
			output += stride;
		}

		last_samples[3] = temp[--idx & 3];
		last_samples[2] = temp[--idx & 3];
		last_samples[1] = temp[--idx & 3];
		last_samples[0] = temp[--idx & 3];
	}
	else if (srctype == RESAMPLING_LINEAR || srctype == RESAMPLING_POLYPHASE)
	{
		// This is the circular buffer containing samples to use for the
		// interpolation. It is initialized with the values from the PB, and it
		// will be stored back to the PB at the end.
		s16 temp[4];
		u32 idx = 0;

		temp[idx++ & 3] = last_samples[0];
		temp[idx++ & 3] = last_samples[1];
		temp[idx++ & 3] = last_samples[2];
		temp[idx++ & 3] = last_samples[3];

		for (u32 i = 0; i < count; ++i)
		{
			curr_pos += ratio;

			// While our current position is >= 1.0, push new samples to the
			// circular buffer.
			while (curr_pos >= 0x10000)
			{
				temp[idx++ & 3] = input_callback(read_samples_count++);
				curr_pos -= 0x10000;
			}

			// Get our current fractional position, used to know how much of
			// curr0 and how much of curr1 the output sample should be.
			u16 curr_frac = curr_pos & 0xFFFF;
			u16 inv_curr_frac = -curr_frac;

			// Interpolate! If curr_frac is 0, we can simply take the last
			// sample without any multiplying.
			s16 sample;
			if (curr_frac)
			{
				s32 s0 = temp[idx++ & 3];
				s32 s1 = temp[idx++ & 3];

				sample = ((s0 * inv_curr_frac) + (s1 * curr_frac)) >> 16;
				idx += 2;
			}
			else
			{
				sample = temp[idx++ & 3];
				idx += 3;
			}

			*output = sample;
			output += stride;
		}

		// Update the four last_samples values.
		last_samples[3] = temp[--idx & 3];
		last_samples[2] = temp[--idx & 3];
		last_samples[1] = temp[--idx & 3];
		last_samples[0] = temp[--idx & 3];
	}
	else // RESAMPLING_NONE
	{
		// No sample rate conversion here: simply read samples from the
		// accelerator to the output buffer.
		for (u32 i = 0; i < count; ++i)
		{
			s16 sample = input_callback(i);
			if (i + 4 >= count)
			{
				last_samples[4 - (count - i)] = sample;
			}
			*output = sample;
			output += stride;
		}
	}

	return curr_pos;
}

static const s16 fake_coeffs[0x8000] = { 0 };

u32 GetCountNeededForResample(u32 count, u32 curr_pos, u32 ratio,
                              ResamplingType srctype)
{
	// We compute the count by calling the resample code with some default
	// input values and counting the number of times the callback is executed.

	s16* fake_output = (s16*)alloca(count * sizeof (s16));
	s16 last_samples[4];

	u32 needed = 0;
	ResampleAudio([&needed](u32) -> s16 { needed += 1; return 0; },
	              fake_output, count, 1, last_samples, curr_pos, ratio,
	              srctype, fake_coeffs);

	return needed;
}

}
