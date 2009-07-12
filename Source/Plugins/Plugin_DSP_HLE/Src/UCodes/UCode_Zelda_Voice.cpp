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

#include "../main.h"
#include "Mixer.h"

void CUCode_Zelda::ReadVoicePB(u32 _Addr, ZeldaVoicePB& PB)
{
	u16 *memory = (u16*)g_dspInitialize.pGetMemoryPointer(_Addr);

	// Perform byteswap
	for (int i = 0; i < (0x180 / 2); i++)
		((u16*)&PB)[i] = Common::swap16(memory[i]);

	// Word swap all 32-bit variables.
	PB.RestartPos = (PB.RestartPos << 16) | (PB.RestartPos >> 16);
	PB.CurAddr = (PB.CurAddr << 16) | (PB.CurAddr >> 16);
	PB.RemLength = (PB.RemLength << 16) | (PB.RemLength >> 16);
	// Read only part
	PB.LoopStartPos = (PB.LoopStartPos << 16) | (PB.LoopStartPos >> 16);
	PB.Length = (PB.Length << 16) | (PB.Length >> 16);
	PB.StartAddr = (PB.StartAddr << 16) | (PB.StartAddr >> 16);
	PB.UnkAddr = (PB.UnkAddr << 16) | (PB.UnkAddr >> 16);
}

void CUCode_Zelda::WritebackVoicePB(u32 _Addr, ZeldaVoicePB& PB)
{
	u16 *memory = (u16*)g_dspInitialize.pGetMemoryPointer(_Addr);

	// Word swap all 32-bit variables.
	PB.RestartPos = (PB.RestartPos << 16) | (PB.RestartPos >> 16);
	PB.CurAddr = (PB.CurAddr << 16) | (PB.CurAddr >> 16);
	PB.RemLength = (PB.RemLength << 16) | (PB.RemLength >> 16);

	// Perform byteswap
	// Only the first 0x100 bytes are written back
	for (int i = 0; i < (0x100 / 2); i++)
		memory[i] = Common::swap16(((u16*)&PB)[i]);
}

int CUCode_Zelda::ConvertRatio(int pb_ratio)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (pb_ratio << 16);
	return (u64)((_ratio * ratioFactor) * 16) >> 16;
}

int CUCode_Zelda::SizeForResampling(ZeldaVoicePB &PB, int size, int ratio) {
	// This is the little calculation at the start of every sample decoder
	// in the ucode.
	return (PB.CurSampleFrac + size * ConvertRatio(PB.RatioInt)) >> 16;
}

// Simple resampler, linear interpolation.
// Any future state should be stored in PB.raw[0x3c to 0x3f].
// In must point 4 samples into a buffer.
void CUCode_Zelda::Resample(ZeldaVoicePB &PB, int size, s16 *in, s32 *out, bool do_resample)
{
	if (!do_resample)
	{
		memcpy(out, in, size * sizeof(int));
		return;
	}

	for (int i = 0; i < 4; i++)
	{
		in[i - 4] = (s16)PB.ResamplerOldData[i];
	}

	int ratio = ConvertRatio(PB.RatioInt);
	int in_size = SizeForResampling(PB, size, ratio);
	
	int position = PB.CurSampleFrac;
	for (int i = 0; i < size; i++)
	{
		int int_pos = (position >> 16);
		int frac = ((position & 0xFFFF) >> 1);
		out[i] = (in[int_pos - 3] * (frac ^ 0x7FFF) + in[int_pos - 2] * frac) >> 15;
		position += ratio;
	}

	for (int i = 0; i < 4; i++)
	{
		PB.ResamplerOldData[i] = (u16)(s16)in[in_size - 4 + i];
	}
	PB.CurSampleFrac = position & 0xFFFF;
}


void CUCode_Zelda::RenderVoice_PCM16(ZeldaVoicePB &PB, s16 *_Buffer, int _Size)
{
	int _RealSize = SizeForResampling(PB, _Size, PB.RatioInt);

	if (PB.KeyOff != 0)
		return;

	if (PB.NeedsReset)
	{
		// 0a7f_UpdateSampleCounters10
		PB.RemLength = PB.Length - PB.RestartPos;
		PB.CurAddr =  PB.StartAddr + (PB.RestartPos << 1);
		PB.ReachedEnd = 0;
		for (int i = 0; i < 4; i++) 
			PB.ResamplerOldData[i] = 0;
	}

	int inpos = 0;
	int outpos = 0;   // Must be before _lRestart

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
			inpos = 0;
		}
	}

	s16 *source;
	if (m_CRC == 0xD643001F)  // SMG
		source = (s16*)(g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr) + PB.CurAddr);
	else
		source = (s16*)(g_dspInitialize.pGetARAMPointer() + PB.CurAddr);

	for (; outpos < _RealSize;)
	{
		_Buffer[outpos++] = (s16)Common::swap16(source[inpos]);
		inpos++;  // hm, above or below the if...
		if ((inpos + ((PB.CurAddr - PB.StartAddr) >> 1)) >= PB.Length)
		{
			PB.ReachedEnd = 1;
			goto _lRestart;
		}
	}

	if (PB.RemLength < inpos)
	{
		PB.RemLength = 0;
		PB.ReachedEnd = 1;
	}
	else
		PB.RemLength -= inpos;

	PB.CurAddr += inpos << 1;
}

void CUCode_Zelda::RenderVoice_AFC(ZeldaVoicePB &PB, s16 *_Buffer, int _Size)
{
	int _RealSize = SizeForResampling(PB, _Size, PB.RatioInt);

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
		PB.CurSampleFrac = 0;

		for (int i = 0; i < 4; i++) 
			PB.ResamplerOldData[i] = 0;
    }

    if (PB.KeyOff != 0)  // 0747 early out... i dunno if this can happen because we filter it above
        return;

	// Round upwards how many samples we need to copy, 0759
    // u32 frac = NumberOfSamples & 0xF;
    // NumberOfSamples = (NumberOfSamples + 0xf) >> 4;   // i think the lower 4 are the fraction

	const u8 *source;
	u32 ram_mask = 1024 * 1024 * 16 - 1;
	if (m_CRC == 0xD643001F) {
		source = g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr);
		ram_mask = 1024 * 1024 * 64 - 1;
	}
	else
		source = g_dspInitialize.pGetARAMPointer();

    int sampleCount = 0;  // must be above restart.

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
		}
	}

	short outbuf[16] = {0};
	u16 prev_yn1 = PB.YN1;
	u16 prev_yn2 = PB.YN2;
	u32 prev_addr = PB.CurAddr;

	// Prefill the decode buffer.
    AFCdecodebuffer(m_AFCCoefTable, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, PB.Format);
	PB.CurAddr += 9;

	u32 SamplePosition = PB.Length - PB.RemLength;
    while (sampleCount < _RealSize)
    {
		_Buffer[sampleCount] = outbuf[SamplePosition & 15];
		sampleCount++;

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

			AFCdecodebuffer(m_AFCCoefTable, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, PB.Format);
			PB.CurAddr += 9;
		}
    }

	// Here we should back off to the previous addr/yn1/yn2, since we didn't consume the full last block.
	// We'll re-decode it the next time around.
	PB.YN2 = prev_yn2;
	PB.YN1 = prev_yn1;
	PB.CurAddr = prev_addr;

    PB.NeedsReset = 0;
	// write back
    // NumberOfSamples = (NumberOfSamples << 4) | frac;    // missing fraction

    // i think  pTest[0x3a] and pTest[0x3b] got an update after you have decoded some samples...
    // just decrement them with the number of samples you have played
    // and increase the ARAM Offset in pTest[0x38], pTest[0x39]

    // end of block (Zelda 03b2)
}


// Researching what's actually inside the mysterious 0x21 case
// 0x21 seems to really just be reading raw 16-bit audio from RAM (not ARAM).
void CUCode_Zelda::RenderVoice_Raw(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (PB.RatioInt << 16);
	s64 ratio = (_ratio * ratioFactor) * 16;

	s64 samples_to_read;
	
	// TODO: De-Ugly
	if (PB.Format == 0x21)       // Resampled
		samples_to_read = (((PB.CurSampleFrac + (PB.RatioInt * 0x50)) << 4) & 0xFFFF0000) >> 8;
	else if (PB.Format == 0x20)  // Unsampled
		samples_to_read = 0x50;
	
	// End of sound
	if (((PB.raw[0x3a] << 16) | PB.raw[0x3b]) <= samples_to_read)
	{
		PB.KeyOff = 1;
		PB.RemLength = 0;
		return;
	}

	if (PB.NeedsReset != 0)
	{
		PB.CurBlock = 0x00;

		// Length in samples.
		PB.RemLength = PB.Length;

		// Copy ARAM addr from r to rw area.
		PB.CurAddr = PB.StartAddr;
		PB.ReachedEnd = 0;
		PB.CurSampleFrac = 0;
	}

	if (PB.KeyOff != 0)
		return;

	u8 *source = g_dspInitialize.pGetMemoryPointer(0x80000000);
	u32 ram_mask = 0x1ffffff;
restart:
	if (PB.ReachedEnd)
	{
		PB.ReachedEnd = 0;

		// HACK: Looping doesn't work.
		if (PB.RepeatMode == 0)
		{
			PB.KeyOff = 1;
			PB.RemLength = 0;
			PB.CurAddr = PB.StartAddr + PB.RestartPos + PB.Length;
			return;
		}
		else
		{
			// This needs adjustment. It was just copied from PCM16.
			PB.RestartPos = PB.LoopStartPos;
			PB.RemLength = PB.Length - PB.RestartPos;
			PB.CurAddr =  PB.StartAddr + (PB.RestartPos << 1);
		}
	}

	u32 prev_addr = PB.CurAddr;

	const u16 *src = (u16 *)(source + (PB.CurAddr & ram_mask));

	s64 TrueSamplePosition = PB.CurSampleFrac; //(s64)(PB.Length - PB.RemLength) << 16;
	//TrueSamplePosition += PB.CurSampleFrac;
	s64 delta = ratio >> 16;  // 0x100000000ULL;
	int sampleCount = 0, realSample = 0;
	while (sampleCount < _Size)
	{
		_Buffer[sampleCount] = realSample >> 3;

		sampleCount++;
		int SamplePosition = TrueSamplePosition >> 16;
		TrueSamplePosition += delta;
		int TargetPosition = TrueSamplePosition >> 16;
		// Decode forwards...
		while (SamplePosition < TargetPosition)
		{
			SamplePosition++;
			realSample = Common::swap16(*src++);
			PB.CurAddr += 2;
		    PB.RemLength--;
			if (PB.RemLength == 0)
			{
				PB.ReachedEnd = 1;
				goto restart;
			}
		}
	}

	PB.NeedsReset = 0;
	PB.CurSampleFrac = TrueSamplePosition & 0xFFFF;
}

void CUCode_Zelda::RenderAddVoice(ZeldaVoicePB &PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size)
{
	if (PB.IsBlank)
	{
		s32 sample = (s32)(s16)PB.FixedSample;
		for (int i = 0; i < _Size; i++)
			m_VoiceBuffer[i] = sample;

		goto ContinueWithBlock;  // Yes, a goto. Yes, it's evil, but it makes the flow look much more like the DSP code.
	}

	// XK: Use this to disable MIDI music (GREAT for testing). Also kills some sound FX.
	//if(PB.SoundType == 0x0d00) {
	//	PB.NeedsReset = 0;
	//	return;
	//}

	// The Resample calls actually don't resample yet.

	// ResampleBuffer corresponds to 0x0580 in ZWW ucode.
	// VoiceBuffer corresponds to 0x0520.

	// First jump table at ZWW: 2a6
	switch (PB.Format)
	{
	case 0x0005:		// AFC with extra low bitrate (32:5 compression). Not yet seen.
		WARN_LOG(DSPHLE, "5 byte AFC - does it work?");

	case 0x0009:		// AFC with normal bitrate (32:9 compression).
		RenderVoice_AFC(PB, m_ResampleBuffer + 4, _Size);
		Resample(PB, _Size, m_ResampleBuffer + 4, m_VoiceBuffer, true);
		break;

	case 0x0008:        // Likely PCM8 - normal PCM 8-bit audio. Used in Mario Kart DD.
		WARN_LOG(DSPHLE, "Unimplemented MixAddVoice format in zelda %04x", PB.Format);
		memset(m_ResampleBuffer + 4, 0, _Size * sizeof(s32));
		Resample(PB, _Size, m_ResampleBuffer + 4, m_VoiceBuffer);
		break;

	case 0x0010:		// PCM16 - normal PCM 16-bit audio.
		RenderVoice_PCM16(PB, m_ResampleBuffer + 4, _Size);
		Resample(PB, _Size, m_ResampleBuffer + 4, m_VoiceBuffer, true);
		break;

	case 0x0020:
		// Normally, this shouldn't resample, it should just decode directly
		// to the output buffer. However, (if we ever see this sound type), we'll
		// have to resample anyway since we're running at a different sample rate.

#if 0   // To hear something weird in ZWW, turn this on.
		// Caution: Use at your own risk. Sounds awful :)
		RenderVoice_Raw(PB, m_ResampleBuffer + 4, _Size);
#else
		// This is what 0x20 and 0x21 do on end of voice
		PB.RemLength = 0;
		PB.KeyOff = 1;
#endif
		Resample(PB, _Size, m_ResampleBuffer + 4, m_VoiceBuffer);
		break;

	case 0x0021:
		// Raw sound from RAM. Important for Zelda WW. Really need to implement - missing it causes hangs.
#if 0   // To hear something weird in ZWW, turn this on.
		// Caution: Use at your own risk. Sounds awful :)
		RenderVoice_Raw(PB, m_ResampleBuffer + 4, _Size);
#else
		// This is what 0x20 and 0x21 do on end of voice
		PB.RemLength = 0;
		PB.KeyOff = 1;
#endif
		Resample(PB, _Size, m_ResampleBuffer + 4, m_VoiceBuffer);
		break;
	
	default:
		// Second jump table
		switch (PB.Format)
		{
		// Synthesized sounds
		case 0x0000: // Example: Magic meter filling up in ZWW
		case 0x0003: 
			RenderSynth_RectWave(PB, m_VoiceBuffer, _Size);
			break;

		case 0x0001: // Example: "Denied" sound when trying to pull out a sword 
					 // indoors in ZWW
			RenderSynth_SawWave(PB, m_VoiceBuffer, _Size);
			break;

		case 0x0006:
			WARN_LOG(DSPHLE, "Synthesizing 0x0006 (constant sound)");
			RenderSynth_Constant(PB, m_VoiceBuffer, _Size);
			break;
                        
  		// These are more "synth" formats - square wave, saw wave etc.
		case 0x0002:
		case 0x000c: // Example: beam of death/yellow force-field in Temple of the Gods, ZWW
			WARN_LOG(DSPHLE, "Synthesizing 0x%04x", PB.Format);
			break;

		default:
			// TODO: Implement general decoder here
			memset(m_VoiceBuffer, 0, _Size * sizeof(s32));
			ERROR_LOG(DSPHLE, "Unknown MixAddVoice format in zelda %04x", PB.Format);
			break;
		}
	}
                    
	// Necessary for SMG, not for Zelda. Weird. Where's it from?
	PB.NeedsReset = 0;

ContinueWithBlock:
	
	if (PB.FilterEnable)
	{  // 0x04a8
		for (int i = 0; i < _Size; i++)
		{
			// TODO: Apply filter from ZWW: 0c84_FilterBufferInPlace
		}
	}

	for (int i = 0; i < _Size; i++)
	{
		
	}
	
	// Apply volume. There are two different modes.
	if (PB.VolumeMode != 0)
	{
		// Complex volume mode. Let's see what we can do.
		if (PB.StopOnSilence) {
			PB.raw[0x2b] = PB.raw[0x2a] >> 1;
			if (PB.raw[0x2b] == 0)
			{
				PB.KeyOff = 1;
			}
		}
		short AX0L = PB.raw[0x28] >> 8;
		short AX0H = PB.raw[0x28] & 0x7F;
		short AX1L = AX0L ^ 0x7F;
		short AX1H = AX0H ^ 0x7F;
		AX0L = m_MiscTable[0x200 + AX0L];
		AX0H = m_MiscTable[0x200 + AX0H];
		AX1L = m_MiscTable[0x200 + AX1L];
		AX1H = m_MiscTable[0x200 + AX1H];

		short b00[16];
		b00[0] = AX1L * AX1H >> 16;
		b00[1] = AX0L * AX1H >> 16;
		b00[2] = AX0H * AX1L >> 16;
		b00[3] = AX0L * AX0H >> 16;

		for (int i = 0; i < 4; i++) {
			b00[i + 4] = (s16)b00[i] * (s16)PB.raw[0x2a] >> 16;
		}

		int prod = ((s16)PB.raw[0x2a] * (s16)PB.raw[0x29] * 2) >> 16;
		for (int i = 0; i < 4; i++) {
			b00[i + 8] = (s16)b00[i + 4] * prod;
		}

		// ZWW 0d34
		
		int diff = (s16)PB.raw[0x2b] - (s16)PB.raw[0x2a];
		PB.raw[0x2a] = PB.raw[0x2b];

		for (int i = 0; i < 4; i++) {
			b00[i + 0xc] = (unsigned short)b00[i] * diff >> 16;
		}

		for (int i = 0; i < 4; i++) {
			b00[i + 0x10] = (s16)b00[i + 0xc] * PB.raw[0x29];
		}

		for (int count = 0; count < 8; count++)
		{
			// The 8 buffers to mix to: 0d00, 0d60, 0f40 0ca0 0e80 0ee0 0c00 0c50
			// We just mix to the first to and call it stereo :p
			int value = b00[0x4 + count];
			int delta = b00[0xC + count] << 11;
			
			int ramp = value << 16;
			for (int i = 0; i < _Size; i++)
			{
				int unmixed_audio = m_VoiceBuffer[i];
				switch (count) {
				case 0: _LeftBuffer[i] += (u64)unmixed_audio * ramp >> 29; break;
				case 1: _RightBuffer[i] += (u64)unmixed_audio * ramp >> 29; break;
					break;
				}
			}
		}
	}
	else
	{
		// ZWW 0355
		if (PB.StopOnSilence)
		{
			int sum = 0;
			int addr = 0x0a;
			for (int i = 0; i < 6; i++)
			{
				u16 value = PB.raw[addr];
				addr--;
				value >>= 1;
				PB.raw[addr] = value;
				sum += value;
				addr += 5;
			}
			if (sum == 0) {
				PB.KeyOff = 1;
			}
		}

		// Seems there are 6 temporary output buffers.
		for (int count = 0; count < 6; count++)
		{
			int addr = 0x08;

			// we'll have to keep a map of buffers I guess...
			u16 dest_buffer_address = PB.raw[addr++];

			bool mix = dest_buffer_address ? true : false;

			u16 vol2 = PB.raw[addr++];
			u16 vol1 = PB.raw[addr++];

			int delta = (vol2 - vol1) << 11;

			addr--;

			u32 ramp = vol1 << 16;
			if (mix)
			{
				// 0ca9_RampedMultiplyAddBuffer
				for (int i = 0; i < _Size; i++)
				{
					int value = m_VoiceBuffer[i];

					// TODO - add to buffer specified by dest_buffer_address
					switch (count)
					{
						// These really should be 32.
						case 0: _LeftBuffer[i] += (u64)value * ramp >> 29; break;
						case 1: _RightBuffer[i] += (u64)value * ramp >> 29; break;
					}
					if ((i & 1) == 0 && i < 64) {
						ramp += delta;
					}
				}
				if (_Size < 32)
				{
					ramp += delta * (_Size - 32);
				}
			}
			// Update the PB with the volume actually reached.
			PB.raw[addr++] = ramp >> 16;
	
			addr++;
		}
	}
}

// size is in stereo samples.
void CUCode_Zelda::MixAdd(short *_Buffer, int _Size)
{
	// Safety check
	if (_Size > 256 * 1024)
		_Size = 256 * 1024;

	// Final mix buffers
	memset(m_LeftBuffer, 0, _Size * sizeof(s32));
	memset(m_RightBuffer, 0, _Size * sizeof(s32));

	// For each PB...
	for (u32 i = 0; i < m_NumVoices; i++)
	{
		u32 flags = m_SyncFlags[(i >> 4) & 0xF];
		if (!(flags & 1 << (15 - (i & 0xF))))
			continue;

		ZeldaVoicePB pb;
		ReadVoicePB(m_VoicePBsAddr + (i * 0x180), pb);

		if (pb.Status == 0)
			continue;
		if (pb.KeyOff != 0)
			continue;

		RenderAddVoice(pb, m_LeftBuffer, m_RightBuffer, _Size);
		WritebackVoicePB(m_VoicePBsAddr + (i * 0x180), pb);
	}

	// Post processing, final conversion.
	for (int i = 0; i < _Size; i++)
	{
		s32 left  = (s32)_Buffer[0] + m_LeftBuffer[i];
		s32 right = (s32)_Buffer[1] + m_RightBuffer[i];

		if (left < -32768) left = -32768;
		if (left > 32767)  left = 32767;
		_Buffer[0] = (short)left;

		if (right < -32768) right = -32768;
		if (right > 32767)  right = 32767;
		_Buffer[1] = (short)right;

		_Buffer += 2;
	}
}