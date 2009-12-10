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
#include "../main.h"
#include "../Config.h"

// ----------------------------------------------------
// Externals
// -----------
extern bool gSSBM;
extern bool gSSBMremedy1;
extern bool gSSBMremedy2;
extern bool gSequenced;
extern bool gVolume;
extern bool gReset;
extern bool gSequenced;
extern float ratioFactor;


template<class ParamBlockType>
inline bool ReadOutPBWii(u32 pbs_address, ParamBlockType& PB)
{
	u32 blockAddr = pbs_address;
	u32 pAddr = 0;

	const short *pSrc = (const short *)g_dspInitialize.pGetMemoryPointer(blockAddr);
	if (!pSrc)
		return false;
	pAddr = blockAddr;
	short *pDest = (short *)&PB;
	for (u32 p = 0; p < sizeof(ParamBlockType) / 2; p++)
	{
		if (p == 6 || p == 7) pDest[p] = pSrc[p]; // control for the u32
		else pDest[p] = Common::swap16(pSrc[p]);

#if defined(HAVE_WX) && HAVE_WX
		#if defined(_DEBUG) || defined(DEBUGFAST)
			if(m_DebuggerFrame) m_DebuggerFrame->gLastBlock = blockAddr + p*2 + 2;  // save last block location
		#endif
#endif
	}

	PB.mixer_control = Common::swap32(PB.mixer_control);
	return true;
}

template<class ParamBlockType>
inline bool WriteBackPBWii(u32 pb_address, ParamBlockType& PB)
//void WriteBackPBsWii(u32 pbs_address, AXParamBlockWii* _pPBs, int _num)
{
	// write back and 'halfword'swap
	short* pSrc  = (short*)&PB;
	short* pDest = (short*)g_dspInitialize.pGetMemoryPointer(pb_address);
	if (!pDest)
		return false;
	PB.mixer_control = Common::swap32(PB.mixer_control);
	for (size_t p = 0; p < sizeof(ParamBlockType) / 2; p++)
	{
		if (p == 6 || p == 7) pDest[p] = pSrc[p]; // control for the u32		
		else pDest[p] = Common::swap16(pSrc[p]);
	}
	return true;
}

template<class ParamBlockType>
inline void MixAddVoice(ParamBlockType &pb, int *templbuffer, int *temprbuffer, int _iSize, bool Wii, u32 _uCode = UCODE_ROM)
{
    ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();

	DoVoiceHacks(pb, Wii);

	// =============
	if (pb.running)
	{
		// Read initial parameters
		// ------------
		//constants
		const u32 ratio = (u32)(((pb.src.ratio_hi << 16) + pb.src.ratio_lo) * ratioFactor);
		u32 sampleEnd = (pb.audio_addr.end_addr_hi << 16) | pb.audio_addr.end_addr_lo;
		u32 loopPos   = (pb.audio_addr.loop_addr_hi << 16) | pb.audio_addr.loop_addr_lo;

		//variables
		u32 samplePos = (pb.audio_addr.cur_addr_hi << 16) | pb.audio_addr.cur_addr_lo;
		u32 frac = pb.src.cur_addr_frac;
		// =============

		// =======================================================================================
		// Handle No-SRC streams - No src streams have pb.src_type == 2 and have pb.src.ratio_hi = 0
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
			// the sound format plays in to, Baten use ADPCM, SC2 use PCM16
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
		// =============

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
					samplePos = loopPos;
					//samplePos = samplePos - sampleEnd + loopPos;
					memset(&pb.updates, 0, sizeof(pb.updates));
					memset(pb.src.last_samples, 0, 8);
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



// ================================================
// Voice hacks
// --------------
template<class ParamBlockType>
inline void DoVoiceHacks(ParamBlockType &pb, bool Wii)
{
	// get necessary values
	const u32 sampleEnd = (pb.audio_addr.end_addr_hi << 16) | pb.audio_addr.end_addr_lo;
	const u32 loopPos   = (pb.audio_addr.loop_addr_hi << 16) | pb.audio_addr.loop_addr_lo;
	const u32 updaddr   = (u32)(pb.updates.data_hi << 16) | pb.updates.data_lo;
	const u16 updpar    = Memory_Read_U16(updaddr);
	const u16 upddata   = Memory_Read_U16(updaddr + 2);

	// =======================================================================================
	/* Fix problems introduced with the SSBM fix. Sometimes when a music stream ended sampleEnd
	would end up outside of bounds while the block was still playing resulting in noise 
	a strange noise. This should take care of that.
	*/
	// ------------
	if (
		(sampleEnd > (0x017fffff * 2) || loopPos > (0x017fffff * 2)) // ARAM bounds in nibbles
		&& gSSBMremedy1
		&& !Wii
		)
	{
		pb.running = 0;

		// also reset all values if it makes any difference
		pb.audio_addr.cur_addr_hi = 0; pb.audio_addr.cur_addr_lo = 0;
		pb.audio_addr.end_addr_hi = 0; pb.audio_addr.end_addr_lo = 0;
		pb.audio_addr.loop_addr_hi = 0; pb.audio_addr.loop_addr_lo = 0;

		pb.src.cur_addr_frac = 0; pb.src.ratio_hi = 0; pb.src.ratio_lo = 0;
		pb.adpcm.pred_scale = 0; pb.adpcm.yn1 = 0; pb.adpcm.yn2 = 0;

		pb.audio_addr.looping = 0;
		pb.adpcm_loop_info.pred_scale = 0;
		pb.adpcm_loop_info.yn1 = 0; pb.adpcm_loop_info.yn2 = 0;
	}

	/*
	// the fact that no settings are reset (except running) after a SSBM type music stream or another
	looping block (for example in Battle Stadium DON) has ended could cause loud garbled sound to be
	played from one or more blocks. Perhaps it was in conjunction with the old sequenced music fix below,
	I'm not sure. This was an attempt to prevent that anyway by resetting all. But I'm not sure if this
	is needed anymore. Please try to play SSBM without it and see if it works anyway.
	*/
	if (
	// detect blocks that have recently been running that we should reset
	pb.running == 0 && pb.audio_addr.looping == 1
	//pb.running == 0 && pb.adpcm_loop_info.pred_scale

	// this prevents us from ruining sequenced music blocks, may not be needed
	/*
	&& !(pb.updates.num_updates[0] || pb.updates.num_updates[1] || pb.updates.num_updates[2]
		|| pb.updates.num_updates[3] || pb.updates.num_updates[4])
	*/	
	&& !(updpar || upddata)

	&& pb.mixer_control == 0 // only use this in SSBM

	&& gSSBMremedy2 // let us turn this fix on and off
	&& !Wii
	)
	{
		// reset the detection values
		pb.audio_addr.looping = 0;
		pb.adpcm_loop_info.pred_scale = 0;
		pb.adpcm_loop_info.yn1 = 0; pb.adpcm_loop_info.yn2 = 0;

		//pb.audio_addr.cur_addr_hi = 0; pb.audio_addr.cur_addr_lo = 0;
		//pb.audio_addr.end_addr_hi = 0; pb.audio_addr.end_addr_lo = 0;
		//pb.audio_addr.loop_addr_hi = 0; pb.audio_addr.loop_addr_lo = 0;

		//pb.src.cur_addr_frac = 0; PBs[i].src.ratio_hi = 0; PBs[i].src.ratio_lo = 0;
		//pb.adpcm.pred_scale = 0; pb.adpcm.yn1 = 0; pb.adpcm.yn2 = 0;
	}
	
	// =============


	// =======================================================================================
	// Reset all values
	// ------------
	if (gReset
		&& (pb.running || pb.audio_addr.looping || pb.adpcm_loop_info.pred_scale)
		)	
	{
		pb.running = 0;

		pb.audio_addr.cur_addr_hi = 0; pb.audio_addr.cur_addr_lo = 0;
		pb.audio_addr.end_addr_hi = 0; pb.audio_addr.end_addr_lo = 0;
		pb.audio_addr.loop_addr_hi = 0; pb.audio_addr.loop_addr_lo = 0;

		pb.src.cur_addr_frac = 0; pb.src.ratio_hi = 0; pb.src.ratio_lo = 0;
		pb.adpcm.pred_scale = 0; pb.adpcm.yn1 = 0; pb.adpcm.yn2 = 0;

		pb.audio_addr.looping = 0;
		pb.adpcm_loop_info.pred_scale = 0;
		pb.adpcm_loop_info.yn1 = 0; pb.adpcm_loop_info.yn2 = 0;
	}
}


#endif  // _UCODE_AX_VOICE_H
