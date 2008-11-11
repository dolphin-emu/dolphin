// Copyright (C) 2003-2008 Dolphin Project.

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

#include "UCode_AX_ADPCM.h"
#include "../main.h"

template<class ParamBlockType>
inline void MixAddVoice(ParamBlockType &pb, int *templbuffer, int *temprbuffer, int _iSize)
{
#ifdef _WIN32
	float ratioFactor = 32000.0f / (float)DSound::DSound_GetSampleRate();
#else
	float ratioFactor = 32000.0f / 44100.0f;
#endif

	// DoVoiceHacks(pb);

	// =============
	if (pb.running)
	{
		// =======================================================================================
		// Read initial parameters
		// ------------
		//constants						
		const u32 ratio     = (u32)(((pb.src.ratio_hi << 16) + pb.src.ratio_lo) * ratioFactor);
		const u32 sampleEnd = (pb.audio_addr.end_addr_hi << 16) | pb.audio_addr.end_addr_lo;
		const u32 loopPos   = (pb.audio_addr.loop_addr_hi << 16) | pb.audio_addr.loop_addr_lo;

		//variables
		u32 samplePos = (pb.audio_addr.cur_addr_hi << 16) | pb.audio_addr.cur_addr_lo;
		u32 frac = pb.src.cur_addr_frac;
		// =============
		
		// =======================================================================================
		// Handle no-src streams - No src streams have pb.src_type == 2 and have pb.src.ratio_hi = 0
		// and pb.src.ratio_lo = 0. We handle that by setting the sampling ratio integer to 1. This
		// makes samplePos update in the correct way. I'm unsure how we are actually supposed to
		// detect that this setting. Updates did not fix this automatically.
		// ---------------------------------------------------------------------------------------
		// Stream settings
			// src_type = 2 (most other games have src_type = 0)
		// ------------
		// Affected games:
			// Baten Kaitos - Eternal Wings (2003)
			// Baten Kaitos - Origins (2006)?
			// Soul Calibur 2: The movie music use src_type 2 but it needs no adjustment, perhaps
			// the sound format plays in to, Baten use ADPCM SC2 use PCM16
		// ------------
		if (pb.src_type == 2 && (pb.src.ratio_hi == 0 && pb.src.ratio_lo == 0))
		{
			pb.src.ratio_hi = 1;
		}
		// =============

		// =======================================================================================
		// Games that use looping to play non-looping music streams - SSBM has info in all
		// pb.adpcm_loop_info parameters but has pb.audio_addr.looping = 0. If we treat these streams
		// like any other looping streams the music works. I'm unsure how we are actually supposed to
		// detect that these kinds of blocks should be looping. It seems like pb.mixer_control == 0 may
		// identify these types of blocks. Updates did not write any looping values.
		// --------------
		if (
			(pb.adpcm_loop_info.pred_scale || pb.adpcm_loop_info.yn1 || pb.adpcm_loop_info.yn2)
			&& pb.mixer_control == 0
			
			)
		{
			pb.audio_addr.looping = 1;
		}
		// ==============

		// =======================================================================================
		// Walk through _iSize. _iSize = numSamples. If the game goes slow _iSize will be higher to
		// compensate for that. _iSize can be as low as 100 or as high as 2000 some cases.
		for (int s = 0; s < _iSize; s++)
		{
			int sample = 0;
			frac += ratio;
			u32 newSamplePos = samplePos + (frac >> 16); //whole number of frac

			// =======================================================================================
			// Process sample format
			// --------------
			switch (pb.audio_addr.sample_format)
			{
			    case AUDIOFORMAT_PCM8:
					// TODO - the linear interpolation code below is somewhat suspicious
				    pb.adpcm.yn2 = pb.adpcm.yn1; //save last sample
				    pb.adpcm.yn1 = ((s8)g_dspInitialize.pARAM_Read_U8(samplePos)) << 8;

				    if (pb.src_type == SRCTYPE_NEAREST)
				    {
					    sample = pb.adpcm.yn1;
				    }
				    else //linear interpolation
				    {
					    sample = (pb.adpcm.yn1 * (u16)frac + pb.adpcm.yn2 * (u16)(0xFFFF - frac)) >> 16;
				    }

				    samplePos = newSamplePos;
				    break;

			    case AUDIOFORMAT_PCM16:
					// TODO - the linear interpolation code below is somewhat suspicious
				    pb.adpcm.yn2 = pb.adpcm.yn1; //save last sample
				    pb.adpcm.yn1 = (s16)(u16)((g_dspInitialize.pARAM_Read_U8(samplePos * 2) << 8) | (g_dspInitialize.pARAM_Read_U8((samplePos * 2 + 1))));
				    if (pb.src_type == SRCTYPE_NEAREST)
					    sample = pb.adpcm.yn1;
				    else //linear interpolation
					    sample = (pb.adpcm.yn1 * (u16)frac + pb.adpcm.yn2 * (u16)(0xFFFF - frac)) >> 16;

				    samplePos = newSamplePos;
				    break;

			    case AUDIOFORMAT_ADPCM:
				    sample = ADPCM_Step(pb.adpcm, samplePos, newSamplePos, frac);
				    break;

			    default:
				    break;
			}
			// ================

			// =======================================================================================
			// Volume control
			frac &= 0xffff;

			int vol = pb.vol_env.cur_volume >> 9;
			sample = sample * vol >> 8;

			if (pb.mixer_control & MIXCONTROL_RAMPING)
			{
				int x = pb.vol_env.cur_volume;
				x += pb.vol_env.cur_volume_delta; // I'm not sure about this, can anybody find a game
				// that use this? Or how does it work?
				if (x < 0) x = 0;
				if (x >= 0x7fff) x = 0x7fff;
				pb.vol_env.cur_volume = x; // maybe not per sample?? :P
			}

			int leftmix  = pb.mixer.volume_left >> 5;
			int rightmix = pb.mixer.volume_right >> 5;
			// ===============
			int left  = sample * leftmix >> 8;
			int right = sample * rightmix >> 8;
			//adpcm has to walk from oldSamplePos to samplePos here
			templbuffer[s] += left;
			temprbuffer[s] += right;

			if (samplePos >= sampleEnd)
			{
				if (pb.audio_addr.looping == 1)
				{
					samplePos = loopPos;
					if (pb.audio_addr.sample_format == AUDIOFORMAT_ADPCM)
					{
						if (!pb.is_stream)
						{
							pb.adpcm.yn1 = pb.adpcm_loop_info.yn1;
							pb.adpcm.yn2 = pb.adpcm_loop_info.yn2;
							pb.adpcm.pred_scale = pb.adpcm_loop_info.pred_scale;
						}
					}
				}
				else
				{
					pb.running = 0;
					break;
				}
			}
		} // end of the _iSize loop

		// Update volume
		if (sizeof(ParamBlockType) == sizeof(AXParamBlock) && m_frame->gVolume) // allow us to turn this off in the debugger
		{
			pb.mixer.volume_left = ADPCM_Vol(pb.mixer.volume_left, pb.mixer.unknown, pb.mixer_control);
			pb.mixer.volume_right = ADPCM_Vol(pb.mixer.volume_right, pb.mixer.unknown2, pb.mixer_control);
		}

		pb.src.cur_addr_frac = (u16)frac;
		pb.audio_addr.cur_addr_hi = samplePos >> 16;
		pb.audio_addr.cur_addr_lo = (u16)samplePos;
	} //if (pb.running)
}

#endif  // _UCODE_AX_VOICE_H
