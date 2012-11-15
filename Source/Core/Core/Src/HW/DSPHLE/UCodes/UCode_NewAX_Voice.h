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

#ifndef _UCODE_NEWAX_VOICE_H
#define _UCODE_NEWAX_VOICE_H

#include "Common.h"
#include "UCode_AXStructs.h"
#include "../../DSP.h"

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
	};

	int* ptrs[9];
};

// Read a PB from MRAM/ARAM
inline bool ReadPB(u32 addr, AXPB& pb)
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
inline bool WritePB(u32 addr, const AXPB& pb)
{
	const u16* src = (const u16*)&pb;
	u16* dst = (u16*)Memory::GetPointer(addr);
	if (!dst)
		return false;

	for (u32 i = 0; i < sizeof (pb) / sizeof (u16); ++i)
		dst[i] = Common::swap16(src[i]);

	return true;
}

// Simulated accelerator state.
static u32 acc_loop_addr, acc_end_addr;
static u32* acc_cur_addr;
static AXPB* acc_pb;

// Sets up the simulated accelerator.
inline void AcceleratorSetup(AXPB* pb, u32* cur_addr)
{
	acc_pb = pb;
	acc_loop_addr = HILO_TO_32(pb->audio_addr.loop_addr);
	acc_end_addr = HILO_TO_32(pb->audio_addr.end_addr);
	acc_cur_addr = cur_addr;
}

// Reads a sample from the simulated accelerator.
inline u16 AcceleratorGetSample()
{
	u16 ret;

	switch (acc_pb->audio_addr.sample_format)
	{
		case 0x00:	// ADPCM
		{
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

	if (*acc_cur_addr >= acc_end_addr)
	{
		if ((*acc_cur_addr & ~0x1F) == (acc_end_addr & ~0x1F))
			*acc_cur_addr = acc_loop_addr;

		// Simulate an ACC overflow interrupt.
		if (acc_pb->audio_addr.looping)
		{
			acc_pb->adpcm.pred_scale = acc_pb->adpcm_loop_info.pred_scale;
			if (!acc_pb->is_stream)
			{
				acc_pb->adpcm.yn1 = acc_pb->adpcm_loop_info.yn1;
				acc_pb->adpcm.yn2 = acc_pb->adpcm_loop_info.yn2;
			}
		}
		else
		{
			acc_pb->running = 0;
		}
	}

	return ret;
}

// Read 32 input samples from ARAM, decoding and converting rate if required.
inline void GetInputSamples(AXPB& pb, s16* samples)
{
	u32 cur_addr = HILO_TO_32(pb.audio_addr.cur_addr);
	AcceleratorSetup(&pb, &cur_addr);

	// TODO: support polyphase interpolation if coefficients are available.
	if (pb.src_type == SRCTYPE_POLYPHASE || pb.src_type == SRCTYPE_LINEAR)
	{
		u32 ratio = HILO_TO_32(pb.src.ratio);

		u32 curr_pos = pb.src.cur_addr_frac;
		u32 real_samples_needed = (32 * ratio + curr_pos) >> 16;
		s16 real_samples[130];		// Max supported ratio is 4

		real_samples[0] = pb.src.last_samples[2];
		real_samples[1] = pb.src.last_samples[3];
		for (u32 i = 0; i < real_samples_needed; ++i)
			real_samples[i + 2] = AcceleratorGetSample();

		for (u32 i = 0; i < 32; ++i)
		{
			u32 curr_int_pos = (curr_pos >> 16);
			s32 curr_frac_pos = curr_pos & 0xFFFF;
			s16 samp1 = real_samples[curr_int_pos];
			s16 samp2 = real_samples[curr_int_pos + 1];

			s16 sample = samp1 + (s16)(((samp2 - samp1) * (s32)curr_frac_pos) >> 16);
			samples[i] = sample;

			curr_pos += ratio;
		}

		if (real_samples_needed >= 2)
			memcpy(pb.src.last_samples, &real_samples[real_samples_needed + 2 - 4], 4 * sizeof (u16));
		else
		{
			memmove(pb.src.last_samples, &pb.src.last_samples[real_samples_needed], (4 - real_samples_needed) * sizeof (u16));
			memcpy(&pb.src.last_samples[4 - real_samples_needed], &real_samples[2], real_samples_needed * sizeof (u16));
		}
		pb.src.cur_addr_frac = curr_pos & 0xFFFF;
	}
	else // SRCTYPE_NEAREST
	{
		for (u32 i = 0; i < 32; ++i)
			samples[i] = AcceleratorGetSample();

		memcpy(pb.src.last_samples, samples + 28, 4 * sizeof (u16));
	}

	pb.audio_addr.cur_addr_hi = (u16)(cur_addr >> 16);
	pb.audio_addr.cur_addr_lo = (u16)(cur_addr & 0xFFFF);
}

// Mix samples to an output buffer, with optional volume ramping.
inline void MixAdd(int* out, const s16* input, u16* pvol, bool ramp)
{
	u16& volume = pvol[0];
	u16 volume_delta = pvol[1];
	if (!ramp)
		volume_delta = 0;

	for (u32 i = 0; i < 32; ++i)
	{
		s64 sample = 2 * (s16)input[i] * (s16)volume;
		out[i] += (s32)(sample >> 16);
		volume += volume_delta;
	}
}

// Process 1ms of audio from a PB and mix it to the buffers.
inline void Process1ms(AXPB& pb, const AXBuffers& buffers)
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

	// HACK: at the moment we don't mix surround into left and right, so always
	// mix left and right in order to have sound even if a game uses surround
	// only.
	//if (pb.mixer_control & MIX_L)
		MixAdd(buffers.left, samples, &pb.mixer.left, pb.mixer_control & MIX_RAMP);
	//if (pb.mixer_control & MIX_R)
		MixAdd(buffers.right, samples, &pb.mixer.right, pb.mixer_control & MIX_RAMP);
	if (pb.mixer_control & MIX_S)
		MixAdd(buffers.surround, samples, &pb.mixer.surround, pb.mixer_control & MIX_RAMP);

	if (pb.mixer_control & MIX_AUXA_L)
		MixAdd(buffers.auxA_left, samples, &pb.mixer.auxA_left, pb.mixer_control & MIX_AUXA_RAMPLR);
	if (pb.mixer_control & MIX_AUXA_R)
		MixAdd(buffers.auxA_right, samples, &pb.mixer.auxA_right, pb.mixer_control & MIX_AUXA_RAMPLR);
	if (pb.mixer_control & MIX_AUXA_S)
		MixAdd(buffers.auxA_surround, samples, &pb.mixer.auxA_surround, pb.mixer_control & MIX_AUXA_RAMPS);

	if (pb.mixer_control & MIX_AUXB_L)
		MixAdd(buffers.auxB_left, samples, &pb.mixer.auxB_left, pb.mixer_control & MIX_AUXB_RAMPLR);
	if (pb.mixer_control & MIX_AUXB_R)
		MixAdd(buffers.auxB_right, samples, &pb.mixer.auxB_right, pb.mixer_control & MIX_AUXB_RAMPLR);
	if (pb.mixer_control & MIX_AUXB_S)
		MixAdd(buffers.auxB_surround, samples, &pb.mixer.auxB_surround, pb.mixer_control & MIX_AUXB_RAMPS);

	// Optionally, phase shift left or right channel to simulate 3D sound.
	if (pb.initial_time_delay.on)
	{
		// TODO
	}
}

#endif // !_UCODE_NEWAX_VOICE_H
