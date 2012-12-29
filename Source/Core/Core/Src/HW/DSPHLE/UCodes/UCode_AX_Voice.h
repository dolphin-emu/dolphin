// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official Git repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// This file is UGLY (full of #ifdef) so that it can be used with both GC and
// Wii version of AX. Maybe it would be better to abstract away the parts that
// can be made common.

#ifndef _UCODE_AX_VOICE_H
#define _UCODE_AX_VOICE_H

#if !defined(AX_GC) && !defined(AX_WII)
#error UCode_AX_Voice.h included without specifying version
#endif

#include "Common.h"
#include "UCode_AX_Structs.h"
#include "../../DSP.h"

#ifdef AX_GC
# define PB_TYPE AXPB
#else
# define PB_TYPE AXPBWii
#endif

// Put all of that in an anonymous namespace to avoid stupid compilers merging
// functions from AX GC and AX Wii.
namespace {

// Useful macro to convert xxx_hi + xxx_lo to xxx for 32 bits.
#define HILO_TO_32(name) \
	((name##_hi << 16) | name##_lo)

// Used to pass a large amount of buffers to the mixing function.
union AXBuffers
{
	struct
	{
		int* left;
		int* right;
		int* surround;

		int* auxA_left;
		int* auxA_right;
		int* auxA_surround;

		int* auxB_left;
		int* auxB_right;
		int* auxB_surround;

#ifdef AX_WII
		int* auxC_left;
		int* auxC_right;
		int* auxC_surround;
#endif
	};

#ifdef AX_GC
	int* ptrs[9];
#else
	int* ptrs[12];
#endif
};

// Read a PB from MRAM/ARAM
bool ReadPB(u32 addr, PB_TYPE& pb)
{
	u16* dst = (u16*)&pb;
	const u16* src = (const u16*)Memory::GetPointer(addr);
	if (!src)
		return false;

	for (u32 i = 0; i < sizeof (pb) / sizeof (u16); ++i)
		dst[i] = Common::swap16(src[i]);

	return true;
}

// Write a PB back to MRAM/ARAM
bool WritePB(u32 addr, const PB_TYPE& pb)
{
	const u16* src = (const u16*)&pb;
	u16* dst = (u16*)Memory::GetPointer(addr);
	if (!dst)
		return false;

	for (u32 i = 0; i < sizeof (pb) / sizeof (u16); ++i)
		dst[i] = Common::swap16(src[i]);

	return true;
}

// Dump the value of a PB for debugging
#define DUMP_U16(field) WARN_LOG(DSPHLE, "    %04x (%s)", pb.field, #field)
#define DUMP_U32(field) WARN_LOG(DSPHLE, "    %08x (%s)", HILO_TO_32(pb.field), #field)
void DumpPB(const PB_TYPE& pb)
{
	DUMP_U32(next_pb);
	DUMP_U32(this_pb);
	DUMP_U16(src_type);
	DUMP_U16(coef_select);
#ifdef AX_GC
	DUMP_U16(mixer_control);
#else
	DUMP_U32(mixer_control);
#endif
	DUMP_U16(running);
	DUMP_U16(is_stream);

	// TODO: complete as needed
}

// Simulated accelerator state.
static u32 acc_loop_addr, acc_end_addr;
static u32* acc_cur_addr;
static PB_TYPE* acc_pb;

// Sets up the simulated accelerator.
void AcceleratorSetup(PB_TYPE* pb, u32* cur_addr)
{
	acc_pb = pb;
	acc_loop_addr = HILO_TO_32(pb->audio_addr.loop_addr);
	acc_end_addr = HILO_TO_32(pb->audio_addr.end_addr);
	acc_cur_addr = cur_addr;
}

// Reads a sample from the simulated accelerator. Also handles looping and
// disabling streams that reached the end (this is done by an exception raised
// by the accelerator on real hardware).
u16 AcceleratorGetSample()
{
	u16 ret;

	switch (acc_pb->audio_addr.sample_format)
	{
		case 0x00:	// ADPCM
		{
			// ADPCM decoding, not much to explain here.
			if ((*acc_cur_addr & 15) == 0)
			{
				acc_pb->adpcm.pred_scale = DSP::ReadARAM((*acc_cur_addr & ~15) >> 1);
				*acc_cur_addr += 2;
			}

			int scale = 1 << (acc_pb->adpcm.pred_scale & 0xF);
			int coef_idx = (acc_pb->adpcm.pred_scale >> 4) & 0x7;

			s32 coef1 = acc_pb->adpcm.coefs[coef_idx * 2 + 0];
			s32 coef2 = acc_pb->adpcm.coefs[coef_idx * 2 + 1];

			int temp = (*acc_cur_addr & 1) ?
					(DSP::ReadARAM(*acc_cur_addr >> 1) & 0xF) :
					(DSP::ReadARAM(*acc_cur_addr >> 1) >> 4);

			if (temp >= 8)
				temp -= 16;

			int val = (scale * temp) + ((0x400 + coef1 * acc_pb->adpcm.yn1 + coef2 * acc_pb->adpcm.yn2) >> 11);

			if (val > 0x7FFF) val = 0x7FFF;
			else if (val < -0x7FFF) val = -0x7FFF;

			acc_pb->adpcm.yn2 = acc_pb->adpcm.yn1;
			acc_pb->adpcm.yn1 = val;
			*acc_cur_addr += 1;
			ret = val;
			break;
		}

		case 0x0A:	// 16-bit PCM audio
			ret = (DSP::ReadARAM(*acc_cur_addr * 2) << 8) | DSP::ReadARAM(*acc_cur_addr * 2 + 1);
			acc_pb->adpcm.yn2 = acc_pb->adpcm.yn1;
			acc_pb->adpcm.yn1 = ret;
			*acc_cur_addr += 1;
			break;

		case 0x19:	// 8-bit PCM audio
			ret = DSP::ReadARAM(*acc_cur_addr) << 8;
			acc_pb->adpcm.yn2 = acc_pb->adpcm.yn1;
			acc_pb->adpcm.yn1 = ret;
			*acc_cur_addr += 1;
			break;

		default:
			ERROR_LOG(DSPHLE, "Unknown sample format: %d", acc_pb->audio_addr.sample_format);
			return 0;
	}

	// Have we reached the end address?
	//
	// On real hardware, this would raise an interrupt that is handled by the
	// UCode. We simulate what this interrupt does here.
	if (*acc_cur_addr >= acc_end_addr)
	{
		// If we are really at the end (and we don't simply have cur_addr >
		// end_addr all the time), loop back to loop_addr.
		if ((*acc_cur_addr & ~0x1F) == (acc_end_addr & ~0x1F))
			*acc_cur_addr = acc_loop_addr;

		if (acc_pb->audio_addr.looping)
		{
			// Set the ADPCM infos to continue processing at loop_addr.
			//
			// For some reason, yn1 and yn2 aren't set if the voice is not of
			// stream type. This is what the AX UCode does and I don't really
			// know why.
			acc_pb->adpcm.pred_scale = acc_pb->adpcm_loop_info.pred_scale;
			if (!acc_pb->is_stream)
			{
				acc_pb->adpcm.yn1 = acc_pb->adpcm_loop_info.yn1;
				acc_pb->adpcm.yn2 = acc_pb->adpcm_loop_info.yn2;
			}
		}
		else
		{
			// Non looping voice reached the end -> running = 0.
			acc_pb->running = 0;
		}
	}

	return ret;
}

// Read 32 input samples from ARAM, decoding and converting rate if required.
void GetInputSamples(PB_TYPE& pb, s16* samples)
{
	u32 cur_addr = HILO_TO_32(pb.audio_addr.cur_addr);
	AcceleratorSetup(&pb, &cur_addr);

	// TODO: support polyphase interpolation if coefficients are available.
	if (pb.src_type == SRCTYPE_POLYPHASE || pb.src_type == SRCTYPE_LINEAR)
	{
		// Convert the input to a higher or lower sample rate using a linear
		// interpolation algorithm. The input to output ratio is set in
		// pb.src.ratio, which is a floating point num stored as a 32b integer:
		//  * Upper 16 bits of the ratio are the integer part
		//  * Lower 16 bits are the decimal part
		u32 ratio = HILO_TO_32(pb.src.ratio);

		// We start getting samples not from sample 0, but 0.<cur_addr_frac>.
		// This avoids discontinuties in the audio stream, especially with very
		// low ratios which interpolate a lot of values between two "real"
		// samples.
		u32 curr_pos = pb.src.cur_addr_frac;

		// These are the two samples between which we interpolate. The initial
		// values are stored in the PB, and we update them when resampling the
		// input data.
		s16 curr0 = pb.src.last_samples[2];
		s16 curr1 = pb.src.last_samples[3];

		for (u32 i = 0; i < 32; ++i)
		{
			// Get our current fractional position, used to know how much of
			// curr0 and how much of curr1 the output sample should be.
			s32 curr_frac_pos = curr_pos & 0xFFFF;

			// Linear interpolation: s1 + (s2 - s1) * pos
			s16 sample = curr0 + (s16)(((curr1 - curr0) * (s32)curr_frac_pos) >> 16);
			samples[i] = sample;

			curr_pos += ratio;

			// While our current position is >= 1.0, shift to the next 2
			// samples for interpolation.
			while ((curr_pos >> 16) != 0)
			{
				curr0 = curr1;
				curr1 = AcceleratorGetSample();
				curr_pos -= 0x10000;
			}
		}

		// Update the two last_samples values in the PB as well as the current
		// position.
		pb.src.last_samples[2] = curr0;
		pb.src.last_samples[3] = curr1;
		pb.src.cur_addr_frac = curr_pos & 0xFFFF;
	}
	else // SRCTYPE_NEAREST
	{
		// No sample rate conversion here: simply read 32 samples from the
		// accelerator to the output buffer.
		for (u32 i = 0; i < 32; ++i)
			samples[i] = AcceleratorGetSample();

		memcpy(pb.src.last_samples, samples + 28, 4 * sizeof (u16));
	}

	// Update current position in the PB.
	pb.audio_addr.cur_addr_hi = (u16)(cur_addr >> 16);
	pb.audio_addr.cur_addr_lo = (u16)(cur_addr & 0xFFFF);
}

// Add samples to an output buffer, with optional volume ramping.
void MixAdd(int* out, const s16* input, u16* pvol, s16* dpop, bool ramp)
{
	u16& volume = pvol[0];
	u16 volume_delta = pvol[1];

	// If volume ramping is disabled, set volume_delta to 0. That way, the
	// mixing loop can avoid testing if volume ramping is enabled at each step,
	// and just add volume_delta.
	if (!ramp)
		volume_delta = 0;

	for (u32 i = 0; i < 32; ++i)
	{
		s64 sample = input[i];
		sample *= volume;
		sample >>= 15;

		out[i] += (s16)sample;
		volume += volume_delta;

		*dpop = (s16)sample;
	}
}

// Process 1ms of audio (32 samples) from a PB and mix it to the buffers.
void Process1ms(PB_TYPE& pb, const AXBuffers& buffers, AXMixControl mctrl)
{
	// If the voice is not running, nothing to do.
	if (!pb.running)
		return;

	// Read input samples, performing sample rate conversion if needed.
	s16 samples[32];
	GetInputSamples(pb, samples);

	// Apply a global volume ramp using the volume envelope parameters.
	for (u32 i = 0; i < 32; ++i)
	{
		s64 sample = 2 * (s16)samples[i] * (s16)pb.vol_env.cur_volume;
		samples[i] = (s16)(sample >> 16);
		pb.vol_env.cur_volume += pb.vol_env.cur_volume_delta;
	}

	// Optionally, execute a low pass filter
	if (pb.lpf.enabled)
	{
		// TODO
	}

	// Mix LRS, AUXA and AUXB depending on mixer_control
	// TODO: Handle DPL2 on AUXB.

	if (mctrl & MIX_L)
		MixAdd(buffers.left, samples, &pb.mixer.left, &pb.dpop.left, mctrl & MIX_L_RAMP);
	if (mctrl & MIX_R)
		MixAdd(buffers.right, samples, &pb.mixer.right, &pb.dpop.right, mctrl & MIX_R_RAMP);
	if (mctrl & MIX_S)
		MixAdd(buffers.surround, samples, &pb.mixer.surround, &pb.dpop.surround, mctrl & MIX_S_RAMP);

	if (mctrl & MIX_AUXA_L)
		MixAdd(buffers.auxA_left, samples, &pb.mixer.auxA_left, &pb.dpop.auxA_left, mctrl & MIX_AUXA_L_RAMP);
	if (mctrl & MIX_AUXA_R)
		MixAdd(buffers.auxA_right, samples, &pb.mixer.auxA_right, &pb.dpop.auxA_right, mctrl & MIX_AUXA_R_RAMP);
	if (mctrl & MIX_AUXA_S)
		MixAdd(buffers.auxA_surround, samples, &pb.mixer.auxA_surround, &pb.dpop.auxA_surround, mctrl & MIX_AUXA_S_RAMP);

	if (mctrl & MIX_AUXB_L)
		MixAdd(buffers.auxB_left, samples, &pb.mixer.auxB_left, &pb.dpop.auxB_left, mctrl & MIX_AUXB_L_RAMP);
	if (mctrl & MIX_AUXB_R)
		MixAdd(buffers.auxB_right, samples, &pb.mixer.auxB_right, &pb.dpop.auxB_right, mctrl & MIX_AUXB_R_RAMP);
	if (mctrl & MIX_AUXB_S)
		MixAdd(buffers.auxB_surround, samples, &pb.mixer.auxB_surround, &pb.dpop.auxB_surround, mctrl & MIX_AUXB_S_RAMP);

#ifdef AX_WII
	if (mctrl & MIX_AUXC_L)
		MixAdd(buffers.auxC_left, samples, &pb.mixer.auxC_left, &pb.dpop.auxC_left, mctrl & MIX_AUXC_L_RAMP);
	if (mctrl & MIX_AUXC_R)
		MixAdd(buffers.auxC_right, samples, &pb.mixer.auxC_right, &pb.dpop.auxC_right, mctrl & MIX_AUXC_R_RAMP);
	if (mctrl & MIX_AUXC_S)
		MixAdd(buffers.auxC_surround, samples, &pb.mixer.auxC_surround, &pb.dpop.auxC_surround, mctrl & MIX_AUXC_S_RAMP);
#endif

	// Optionally, phase shift left or right channel to simulate 3D sound.
	if (pb.initial_time_delay.on)
	{
		// TODO
	}
}

} // namespace

#endif // !_UCODE_AX_VOICE_H
