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

// ----------------------------------------------------
// Externals
// -----------
extern bool gSequenced;
extern bool gVolume;
extern float ratioFactor;


template<class ParamBlockType>
inline int ReadOutPBsWii(u32 pbs_address, ParamBlockType& _pPBs, int _num, int _deb)
//int ReadOutPBsWii(u32 pbs_address, AXParamBlockWii* _pPBs, int _num)
{
	int count = 0;
	u32 blockAddr = pbs_address;
	u32 pAddr = 0;

	// reading and 'halfword' swap
	for (int i = 0; i < _num; i++)
	{
		const short *pSrc = (const short *)g_dspInitialize.pGetMemoryPointer(blockAddr);
		pAddr = blockAddr;

		if (pSrc != NULL)
		{
			short *pDest = (short *)&_pPBs[i];
			for (int p = 0; p < sizeof(AXParamBlockWii) / 2; p++)
			{
				if(p == 6 || p == 7) pDest[p] = pSrc[p]; // control for the u32
				else pDest[p] = Common::swap16(pSrc[p]);

				#if defined(_DEBUG) || defined(DEBUGFAST)
					gLastBlock = blockAddr + p*2 + 2;  // save last block location
				#endif
			}

			_pPBs[i].mixer_control = Common::swap32(_pPBs[i].mixer_control);
			blockAddr = (_pPBs[i].next_pb_hi << 16) | _pPBs[i].next_pb_lo;
			count++;
			
			// Detect the last mail by checking when next_pb = 0
			u32 next_pb = (Common::swap16(pSrc[0]) << 16) | Common::swap16(pSrc[1]);
			if(next_pb == 0) break;
		}
		else
			break;
	}

	// return the number of read PBs
	return count;
}

template<class ParamBlockType>
inline void WriteBackPBsWii(u32 pbs_address, ParamBlockType& _pPBs, int _num)
//void WriteBackPBsWii(u32 pbs_address, AXParamBlockWii* _pPBs, int _num)
{
	u32 blockAddr = pbs_address;

	// write back and 'halfword'swap
	for (int i = 0; i < _num; i++)
	{
		short* pSrc  = (short*)&_pPBs[i];
		short* pDest = (short*)g_dspInitialize.pGetMemoryPointer(blockAddr);
		_pPBs[i].mixer_control = Common::swap32(_pPBs[i].mixer_control);
		for (size_t p = 0; p < sizeof(AXParamBlockWii) / 2; p++)
		{
			if(p == 6 || p == 7) pDest[p] = pSrc[p]; // control for the u32		
			else pDest[p] = Common::swap16(pSrc[p]);
		}

		// next block		
		blockAddr = (_pPBs[i].next_pb_hi << 16) | _pPBs[i].next_pb_lo;
	}
}


template<class ParamBlockType>
inline void MixAddVoice(ParamBlockType &pb, int *templbuffer, int *temprbuffer, int _iSize)
{
#ifdef _WIN32
	ratioFactor = 32000.0f / (float)DSound::DSound_GetSampleRate();
#else
	ratioFactor = 32000.0f / 44100.0f;
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
		u32 sampleEnd = (pb.audio_addr.end_addr_hi << 16) | pb.audio_addr.end_addr_lo;
		u32 loopPos   = (pb.audio_addr.loop_addr_hi << 16) | pb.audio_addr.loop_addr_lo;

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
		//if (pb.src_type == 2 && (pb.src.ratio_hi == 0 && pb.src.ratio_lo == 0))
		if (pb.running && (pb.src.ratio_hi == 0 && pb.src.ratio_lo == 0))
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

		// Top Spin 3 Wii
		if(pb.audio_addr.sample_format > 25) pb.audio_addr.sample_format = 0;

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

			// ===================================================================
			// Overall volume control. In addition to this there is also separate volume settings to
			// different channels (left, right etc).
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
			int left  = sample * leftmix >> 8;
			int right = sample * rightmix >> 8;
			//adpcm has to walk from oldSamplePos to samplePos here
			templbuffer[s] += left;
			temprbuffer[s] += right;
			// ===============


			// ===================================================================
			// Control the behavior when we reach the end of the sample
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
			// ===============
		} // end of the _iSize loop

		// Update volume
		//if (sizeof(ParamBlockType) == sizeof(AXParamBlock)) // this is not needed anymore I think
		if (gVolume) // allow us to turn this off in the debugger
		{
			pb.mixer.volume_left = ADPCM_Vol(pb.mixer.volume_left, pb.mixer.unknown);
			pb.mixer.volume_right = ADPCM_Vol(pb.mixer.volume_right, pb.mixer.unknown2);
		}

		pb.src.cur_addr_frac = (u16)frac;
		pb.audio_addr.cur_addr_hi = samplePos >> 16;
		pb.audio_addr.cur_addr_lo = (u16)samplePos;
		
	} // if (pb.running)
}

#endif  // _UCODE_AX_VOICE_H
