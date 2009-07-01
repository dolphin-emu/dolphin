// Copyright (C) 2003-2009 Dolphin Project.

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

#include "../Globals.h"
#include "UCodes.h"
#include "UCode_Zelda.h"
#include "UCode_Zelda_ADPCM.h"

#include "../main.h"
#include "Mixer.h"

void CUCode_Zelda::ReadVoicePB(u32 _Addr, ZeldaVoicePB& PB)
{
	u16 *memory = (u16*)g_dspInitialize.pGetMemoryPointer(_Addr);

	// Perform byteswap
	for (int i = 0; i < (0x180 / 2); i++)
		((u16*)&PB)[i] = Common::swap16(memory[i]);

	PB.RestartPos = (PB.RestartPos << 16) | (PB.RestartPos >> 16);
	PB.CurAddr = (PB.CurAddr << 16) | (PB.CurAddr >> 16);
	PB.RemLength = (PB.RemLength << 16) | (PB.RemLength >> 16);
	PB.LoopStartPos = (PB.LoopStartPos << 16) | (PB.LoopStartPos >> 16);
	PB.Length = (PB.Length << 16) | (PB.Length >> 16);
	PB.StartAddr = (PB.StartAddr << 16) | (PB.StartAddr >> 16);
	PB.UnkAddr = (PB.UnkAddr << 16) | (PB.UnkAddr >> 16);
}

void CUCode_Zelda::WritebackVoicePB(u32 _Addr, ZeldaVoicePB& PB)
{
	u16 *memory = (u16*)g_dspInitialize.pGetMemoryPointer(_Addr);

	PB.RestartPos = (PB.RestartPos << 16) | (PB.RestartPos >> 16);
	PB.CurAddr = (PB.CurAddr << 16) | (PB.CurAddr >> 16);
	PB.RemLength = (PB.RemLength << 16) | (PB.RemLength >> 16);

	// Perform byteswap
	// Only the first 0x100 bytes are written back
	for (int i = 0; i < (0x100 / 2); i++)
		memory[i] = Common::swap16(((u16*)&PB)[i]);
}

void CUCode_Zelda::MixAddVoice_PCM16(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (((PB.RatioInt * 80) + PB.RatioFrac) << 4) & 0xFFFF0000;
	u64 ratio = (u64)(((_ratio / 80) << 16) * ratioFactor);

	u32 pos[2] = {0, 0};
	int i = 0;

	if (PB.KeyOff != 0)
		return;

	if (PB.NeedsReset)
	{
		PB.RemLength = PB.Length - PB.RestartPos;
		PB.CurAddr =  PB.StartAddr + (PB.RestartPos << 1);
		PB.ReachedEnd = 0;
	}

_lRestart:
	if (PB.ReachedEnd)
	{
		PB.ReachedEnd = 0;

		if (PB.RepeatMode == 0)
		{
			PB.KeyOff = 1;
			PB.RemLength = 0;
			PB.CurAddr = PB.StartAddr + (PB.RestartPos << 1) + PB.Length;
			return;
		}
		else
		{
			PB.RestartPos = PB.LoopStartPos;
			PB.RemLength = PB.Length - PB.RestartPos;
			PB.CurAddr =  PB.StartAddr + (PB.RestartPos << 1);
			pos[1] = 0; pos[0] = 0;
		}
	}

	s16 *source;
	if (m_CRC == 0xD643001F)
		source = (s16*)(g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr) + PB.CurAddr);
	else
		source = (s16*)(g_dspInitialize.pGetARAMPointer() + PB.CurAddr);

	for (; i < _Size;)
	{
		s16 sample = Common::swap16(source[pos[1]]);

		_Buffer[i++] = (s32)sample;

		(*(u64*)&pos) += ratio;
		if ((pos[1] + ((PB.CurAddr - PB.StartAddr) >> 1)) >= PB.Length)
		{
			PB.ReachedEnd = 1;
			goto _lRestart;
		}
	}

	if (PB.RemLength < pos[1])
	{
		PB.RemLength = 0;
		PB.ReachedEnd = 1;
	}
	else
		PB.RemLength -= pos[1];

	PB.CurAddr += pos[1] << 1;
	// There should be a position fraction as well.
}

void CUCode_Zelda::MixAddVoice_AFC(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (PB.RatioInt<<16) + PB.RatioFrac;
	s64 ratio = (_ratio * ratioFactor) * 16; // (s64)(((_ratio / 80) << 16) * ratioFactor);

	// initialize "decoder" if the sample is played the first time
    if (PB.NeedsReset != 0)
    {
		// This is 0717_ReadOutPBStuff

		// increment 4fb

        // zelda: 
        // perhaps init or "has played before"
        PB.CurBlock = 0x00;
		PB.YN2 = 0x00;     // history1 
        PB.YN1 = 0x00;     // history2 

		// Length in samples.
        PB.RemLength = PB.Length;

        // Copy ARAM addr from r to rw area.
        PB.CurAddr = PB.StartAddr;
		PB.ReachedEnd = 0;
    }

    if (PB.KeyOff != 0)  // 0747 early out... i dunno if this can happen because we filter it above
        return;

	// round upwards how many samples we need to copy, 0759
    // u32 frac = NumberOfSamples & 0xF;
    // NumberOfSamples = (NumberOfSamples + 0xf) >> 4;   // i think the lower 4 are the fraction

    u32 sampleCount = 0;

	u8 *source;
	u32 ram_mask = 1024 * 1024 * 16 - 1;
	if (m_CRC == 0xD643001F) {
		source = g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr);
		ram_mask = 1024 * 1024 * 64 - 1;
	}
	else
		source = g_dspInitialize.pGetARAMPointer();

	// It must be something like this:

	// The PB contains a small sample buffer of 0x4D decoded samples.
	// If it's empty or "used", decode to it.
	// Then, resample from this buffer to the output as you go. When it needs
	// wrapping, decode more.

restart:
	if (PB.ReachedEnd)
	{
		PB.ReachedEnd = 0;

		// HACK: Looping doesn't work.
		if (true || PB.RepeatMode == 0)
		{
			PB.KeyOff = 1;
			PB.RemLength = 0;
			PB.CurAddr = PB.StartAddr + PB.RestartPos + PB.Length;
			return;
		}
		else
		{
			// This needs adjustment. It's not right for AFC, was just copied from PCM16.
			// We should also probably reinitialize YN1 and YN2 with something - but with what?
			PB.RestartPos = PB.LoopStartPos;
			PB.RemLength = PB.Length - PB.RestartPos;
			PB.CurAddr =  PB.StartAddr + (PB.RestartPos << 1);
//			pos[1] = 0; pos[0] = 0;
		}
	}

	short outbuf[16] = {0};

	u16 prev_yn1 = PB.YN1;
	u16 prev_yn2 = PB.YN2;
	u32 prev_addr = PB.CurAddr;

	// Prefill the decode buffer.
    AFCdecodebuffer(m_AFCCoefTable, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, 9);
	PB.CurAddr += 9;

	s64 TrueSamplePosition = (s64)(PB.Length - PB.RemLength) << 32;
	s64 delta = ratio;  // 0x100000000ULL;
    while (sampleCount < _Size)
    {
		int SamplePosition = TrueSamplePosition >> 32;
		_Buffer[sampleCount] = outbuf[SamplePosition & 15];

		sampleCount++;
		TrueSamplePosition += delta;

		int TargetPosition = TrueSamplePosition >> 32;

		// Decode forwards...
		while (SamplePosition < TargetPosition)
		{
			SamplePosition++;
		    PB.RemLength--;
			if (PB.RemLength == 0)
			{
				PB.ReachedEnd = 1;
				goto restart;
			}

			// Need new samples!
			if ((SamplePosition & 15) == 0) {
				prev_yn1 = PB.YN1;
				prev_yn2 = PB.YN2;
				prev_addr = PB.CurAddr;

				AFCdecodebuffer(m_AFCCoefTable, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, 9);
				PB.CurAddr += 9;
			}
		}
    }

	// Here we should back off to the previous addr/yn1/yn2, since we didn't consume the full last block.
	// We'll have to re-decode it the next time around.
	// if (SamplePosition & 15) {
		PB.YN2 = prev_yn2;
		PB.YN1 = prev_yn1;
		PB.CurAddr = prev_addr;
	// }

    PB.NeedsReset = 0;

	// write back
    // NumberOfSamples = (NumberOfSamples << 4) | frac;    // missing fraction

    // i think  pTest[0x3a] and pTest[0x3b] got an update after you have decoded some samples...
    // just decrement them with the number of samples you have played
    // and incrrease the ARAM Offset in pTest[0x38], pTest[0x39]

    // end of block (Zelda 03b2)
}

void CUCode_Zelda::MixAddVoice(ZeldaVoicePB &PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size)
{
	memset(m_TempBuffer, 0, _Size * sizeof(s32));

	if (PB.IsBlank)
	{
		s32 sample = (s32)(s16)PB.FixedSample;
		for (int i = 0; i < _Size; i++)
			m_TempBuffer[i] = sample;
	}
	else
	{
		switch (PB.Format)
		{
		// Synthesized sounds
		case 0x0000: // Example: Magic meter filling up in ZWW
		case 0x0001: // Example: "Denied" sound when trying to pull out a sword 
			         // indoors in ZWW
			MixAddSynth_Waveform(PB, m_TempBuffer, _Size);
			break;

		case 0x0006:
			WARN_LOG(DSPHLE, "Synthesizing 0x0006 (constant sound)");
			MixAddSynth_Constant(PB, m_TempBuffer, _Size);
			break;
                        
      	// These are more "synth" formats - square wave, saw wave etc.
		case 0x0002:          
			WARN_LOG(DSPHLE, "Synthesizing 0x0002");
			break;

                    
		// AFC formats
		case 0x0005:		// AFC with extra low bitrate (32:5 compression). Not yet seen.
			WARN_LOG(DSPHLE, "5 byte AFC - does it work?");
		case 0x0009:		// AFC with normal bitrate (32:9 compression).
			MixAddVoice_AFC(PB, m_TempBuffer, _Size);
			break;

		case 0x0010:		// PCM16 - normal PCM 16-bit audio.
			MixAddVoice_PCM16(PB, m_TempBuffer, _Size);
			break;


		case 0x0008:   // Likely PCM8 - normal PCM 8-bit audio. Used in Mario Kart DD.
		case 0x0020:
		case 0x0021:   // Important for Zelda WW. Really need to implement - missing it causes hangs.
			WARN_LOG(DSPHLE, "Unimplemented MixAddVoice format in zelda %04x", PB.Format);
			break;

		default:
			// TODO: Implement general decoder here
			ERROR_LOG(DSPHLE, "Unknown MixAddVoice format in zelda %04x", PB.Format);
			break;
		}

		// Necessary for SMG, not for Zelda. Weird.
		PB.NeedsReset = 0;
	}

	for (int i = 0; i < _Size; i++)
	{
		s32 left = _LeftBuffer[i] + m_TempBuffer[i];
		s32 right = _RightBuffer[i] + m_TempBuffer[i];

		if (left < -32768) left = -32768;
		if (left > 32767)  left = 32767;
		_LeftBuffer[i] = left;

		if (right < -32768) right = -32768;
		if (right > 32767)  right = 32767;
		_RightBuffer[i] = right;
	}
}
