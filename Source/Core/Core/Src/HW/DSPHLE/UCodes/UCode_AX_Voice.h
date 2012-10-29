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

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _UCODE_AX_VOICE_H
#define _UCODE_AX_VOICE_H

#include "UCodes.h"
#include "UCode_AX_ADPCM.h"
#include "UCode_AX.h"
#include "Mixer.h"
#include "../../AudioInterface.h"

// MRAM -> ARAM for GC
inline bool ReadPB(u32 addr, AXPB &PB)
{
	const u16* PB_in_mram = (const u16*)Memory::GetPointer(addr);
	if (PB_in_mram == NULL)
		return false;
	u16* PB_in_aram = (u16*)&PB;

	for (size_t p = 0; p < (sizeof(AXPB) >> 1); p++)
	{
		PB_in_aram[p] = Common::swap16(PB_in_mram[p]);
	}

	return true;
}

// MRAM -> ARAM for Wii
inline bool ReadPB(u32 addr, AXPBWii &PB)
{
	const u16* PB_in_mram = (const u16*)Memory::GetPointer(addr);
	if (PB_in_mram == NULL)
		return false;
	u16* PB_in_aram = (u16*)&PB;

	// preswap the mixer_control
	PB.mixer_control = ((u32)PB_in_mram[7] << 16) | ((u32)PB_in_mram[6] >> 16);

	for (size_t p = 0; p < (sizeof(AXPBWii) >> 1); p++)
	{
		PB_in_aram[p] = Common::swap16(PB_in_mram[p]);
	}

	return true;
}

// ARAM -> MRAM for GC
inline bool WritePB(u32 addr, AXPB &PB)
{
	const u16* PB_in_aram  = (const u16*)&PB;
	u16* PB_in_mram = (u16*)Memory::GetPointer(addr);
	if (PB_in_mram == NULL)
		return false;

	for (size_t p = 0; p < (sizeof(AXPB) >> 1); p++)
	{
		PB_in_mram[p] = Common::swap16(PB_in_aram[p]);
	}

	return true;
}

// ARAM -> MRAM for Wii
inline bool WritePB(u32 addr, AXPBWii &PB)
{
	const u16* PB_in_aram  = (const u16*)&PB;
	u16* PB_in_mram = (u16*)Memory::GetPointer(addr);
	if (PB_in_mram == NULL)
		return false;

	// preswap the mixer_control
	*(u32*)&PB_in_mram[6] = (PB.mixer_control << 16) | (PB.mixer_control >> 16);

	for (size_t p = 0; p < (sizeof(AXPBWii) >> 1); p++)
	{
		PB_in_mram[p] = Common::swap16(PB_in_aram[p]);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// TODO: fix handling of gc/wii PB differences
// TODO: generally fix up the mess - looks crazy and kinda wrong
template<class ParamBlockType>
inline void MixAddVoice(ParamBlockType &pb,
						int *templbuffer, int *temprbuffer,
						int _iSize)
{
	if (pb.running)
	{
		const u32 ratio = (u32)(((pb.src.ratio_hi << 16) + pb.src.ratio_lo)
			* /*ratioFactor:*/((float)AudioInterface::GetAIDSampleRate() / (float)soundStream->GetMixer()->GetSampleRate()));
		u32 sampleEnd = (pb.audio_addr.end_addr_hi << 16) | pb.audio_addr.end_addr_lo;
		u32 loopPos   = (pb.audio_addr.loop_addr_hi << 16) | pb.audio_addr.loop_addr_lo;

		u32 samplePos = (pb.audio_addr.cur_addr_hi << 16) | pb.audio_addr.cur_addr_lo;
		u32 frac = pb.src.cur_addr_frac;

		// =======================================================================================
		// Handle No-SRC streams - No src streams have pb.src_type == 2 and have pb.src.ratio_hi = 0
		// and pb.src.ratio_lo = 0. We handle that by setting the sampling ratio integer to 1. This
		// makes samplePos update in the correct way. I'm unsure how we are actually supposed to
		// detect that this setting. Updates did not fix this automatically.
		// ---------------------------------------------------------------------------------------
		// Stream settings
		// src_type = 2 (most other games have src_type = 0)
		// Affected games:
		// Baten Kaitos - Eternal Wings (2003)
		// Baten Kaitos - Origins (2006)?
		// Soul Calibur 2: The movie music use src_type 2 but it needs no adjustment, perhaps
		// the sound format plays in to, Baten use ADPCM, SC2 use PCM16
		//if (pb.src_type == 2 && (pb.src.ratio_hi == 0 && pb.src.ratio_lo == 0))
		if (pb.running && (pb.src.ratio_hi == 0 && pb.src.ratio_lo == 0))
		{
			pb.src.ratio_hi = 1;
		}

		// =======================================================================================
		// Games that use looping to play non-looping music streams - SSBM has info in all
		// pb.adpcm_loop_info parameters but has pb.audio_addr.looping = 0. If we treat these streams
		// like any other looping streams the music works. I'm unsure how we are actually supposed to
		// detect that these kinds of blocks should be looping. It seems like pb.mixer_control == 0 may
		// identify these types of blocks. Updates did not write any looping values.
		if (
			(pb.adpcm_loop_info.pred_scale || pb.adpcm_loop_info.yn1 || pb.adpcm_loop_info.yn2)
			&& pb.mixer_control == 0 && pb.adpcm_loop_info.pred_scale <= 0x7F
		   )
		{
			pb.audio_addr.looping = 1;
		}



		// Top Spin 3 Wii
		if (pb.audio_addr.sample_format > 25)
			pb.audio_addr.sample_format = 0;

		// =======================================================================================
		// Walk through _iSize. _iSize = numSamples. If the game goes slow _iSize will be higher to
		// compensate for that. _iSize can be as low as 100 or as high as 2000 some cases.
		for (int s = 0; s < _iSize; s++)
		{
			int sample = 0;
			u32 oldFrac = frac;
			frac += ratio;
			u32 newSamplePos = samplePos + (frac >> 16); //whole number of frac

			// =======================================================================================
			// Process sample format
			switch (pb.audio_addr.sample_format)
			{
			    case AUDIOFORMAT_PCM8:
				    pb.adpcm.yn2 = ((s8)DSP::ReadARAM(samplePos)) << 8; //current sample
				    pb.adpcm.yn1 = ((s8)DSP::ReadARAM(samplePos + 1)) << 8; //next sample

				    if (pb.src_type == SRCTYPE_NEAREST)
					    sample = pb.adpcm.yn2;
				    else // linear interpolation
						sample = (pb.adpcm.yn1 * (u16)oldFrac + pb.adpcm.yn2 * (u16)(0xFFFF - oldFrac) + pb.adpcm.yn2) >> 16;

				    samplePos = newSamplePos;
				    break;

			    case AUDIOFORMAT_PCM16:
				    pb.adpcm.yn2 = (s16)(u16)((DSP::ReadARAM(samplePos * 2) << 8) | (DSP::ReadARAM((samplePos * 2 + 1)))); //current sample
				    pb.adpcm.yn1 = (s16)(u16)((DSP::ReadARAM((samplePos + 1) * 2) << 8) | (DSP::ReadARAM(((samplePos + 1) * 2 + 1)))); //next sample

					if (pb.src_type == SRCTYPE_NEAREST)
					    sample = pb.adpcm.yn2;
				    else // linear interpolation
					    sample = (pb.adpcm.yn1 * (u16)oldFrac + pb.adpcm.yn2 * (u16)(0xFFFF - oldFrac) + pb.adpcm.yn2) >> 16;

				    samplePos = newSamplePos;
				    break;

			    case AUDIOFORMAT_ADPCM:
					ADPCM_Step(pb.adpcm, samplePos, newSamplePos, frac);
					
					if (pb.src_type == SRCTYPE_NEAREST)
					    sample = pb.adpcm.yn2;
				    else // linear interpolation
					    sample = (pb.adpcm.yn1 * (u16)frac + pb.adpcm.yn2 * (u16)(0xFFFF - frac) + pb.adpcm.yn2) >> 16; //adpcm moves on frac

				    break;

			    default:
				    break;
			}

			// ===================================================================
			// Overall volume control. In addition to this there is also separate volume settings to
			// different channels (left, right etc).
			frac &= 0xffff;

			int vol = pb.vol_env.cur_volume >> 9;
			sample = sample * vol >> 8;

			if (pb.mixer_control & MIXCONTROL_RAMPING)
			{
				int x = pb.vol_env.cur_volume;
				x += pb.vol_env.cur_volume_delta;	// I'm not sure about this, can anybody find a game
													// that use this? Or how does it work?
				if (x < 0)
					x = 0;
				if (x >= 0x7fff)
					x = 0x7fff;
				pb.vol_env.cur_volume = x; // maybe not per sample?? :P
			}

			int leftmix		= pb.mixer.left >> 5;
			int rightmix	= pb.mixer.right >> 5;
			int left		= sample * leftmix >> 8;
			int right		= sample * rightmix >> 8;
			// adpcm has to walk from oldSamplePos to samplePos here
			templbuffer[s] += left;
			temprbuffer[s] += right;

			// Control the behavior when we reach the end of the sample
			if (samplePos >= sampleEnd)
			{
				if (pb.audio_addr.looping == 1)
				{
					if ((samplePos & ~0x1f) == (sampleEnd & ~0x1f) || (pb.audio_addr.sample_format != AUDIOFORMAT_ADPCM))
						samplePos = loopPos;
					if ((!pb.is_stream) && (pb.audio_addr.sample_format == AUDIOFORMAT_ADPCM))
					{
						pb.adpcm.yn1 = pb.adpcm_loop_info.yn1;
						pb.adpcm.yn2 = pb.adpcm_loop_info.yn2;
						pb.adpcm.pred_scale = pb.adpcm_loop_info.pred_scale;
					}
				}
				else
				{
					pb.running = 0;
					samplePos = loopPos;
					//samplePos = samplePos - sampleEnd + loopPos;
					memset(&pb.dpop, 0, sizeof(pb.dpop));
					memset(pb.src.last_samples, 0, 8);
					break;
				}
			}
		} // end of the _iSize loop

		// Update volume
		pb.mixer.left = ADPCM_Vol(pb.mixer.left, pb.mixer.left_delta);
		pb.mixer.right = ADPCM_Vol(pb.mixer.right, pb.mixer.right_delta);

		pb.src.cur_addr_frac = (u16)frac;
		pb.audio_addr.cur_addr_hi = samplePos >> 16;
		pb.audio_addr.cur_addr_lo = (u16)samplePos;
		
	} // if (pb.running)
}

#endif
