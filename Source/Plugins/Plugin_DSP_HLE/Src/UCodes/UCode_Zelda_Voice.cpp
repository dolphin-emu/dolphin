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

void CUCode_Zelda::RenderVoice_PCM16(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (((PB.RatioInt * 80) + PB.CurSampleFrac) << 4) & 0xFFFF0000;
	u64 ratio = (u64)(((_ratio / 80) << 16) * ratioFactor);

	u32 inpos[2] = {0, 0};
	int outpos = 0;

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
			inpos[1] = 0; inpos[0] = 0;
		}
	}

	s16 *source;
	if (m_CRC == 0xD643001F)
		source = (s16*)(g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr) + PB.CurAddr);
	else
		source = (s16*)(g_dspInitialize.pGetARAMPointer() + PB.CurAddr);

	for (; outpos < _Size;)
	{
		s16 sample = Common::swap16(source[inpos[1]]);

		_Buffer[outpos++] = (s32)sample;

		(*(u64*)&inpos) += ratio;
		if ((inpos[1] + ((PB.CurAddr - PB.StartAddr) >> 1)) >= PB.Length)
		{
			PB.ReachedEnd = 1;
			goto _lRestart;
		}
	}

	if (PB.RemLength < inpos[1])
	{
		PB.RemLength = 0;
		PB.ReachedEnd = 1;
	}
	else
		PB.RemLength -= inpos[1];

	PB.CurAddr += inpos[1] << 1;
	// There should be a position fraction as well.
}

void CUCode_Zelda::RenderVoice_AFC(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (PB.RatioInt << 16);//  + PB.RatioFrac;
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
		PB.CurSampleFrac = 0;
		// Looking at Zelda Four Swords
		// WARN_LOG(DSPHLE, "PB -----: %04x", PB.Unk03);
		// WARN_LOG(DSPHLE, "PB Unk03: %04x", PB.Unk03);      0
		// WARN_LOG(DSPHLE, "PB Unk07: %04x", PB.Unk07[0]);   0

		/// WARN_LOG(DSPHLE, "PB Unk78: %04x", PB.Unk78);
		// WARN_LOG(DSPHLE, "PB Unk79: %04x", PB.Unk79);
		// WARN_LOG(DSPHLE, "PB Unk31: %04x", PB.Unk31);
		// WARN_LOG(DSPHLE, "PB Unk36: %04x", PB.Unk36[0]);
		// WARN_LOG(DSPHLE, "PB Unk37: %04x", PB.Unk36[1]);
		// WARN_LOG(DSPHLE, "PB Unk3c: %04x", PB.Unk3C[0]);
		// WARN_LOG(DSPHLE, "PB Unk3d: %04x", PB.Unk3C[1]);
    }

    if (PB.KeyOff != 0)  // 0747 early out... i dunno if this can happen because we filter it above
        return;

	// round upwards how many samples we need to copy, 0759
    // u32 frac = NumberOfSamples & 0xF;
    // NumberOfSamples = (NumberOfSamples + 0xf) >> 4;   // i think the lower 4 are the fraction

	u8 *source;
	u32 ram_mask = 1024 * 1024 * 16 - 1;
	if (m_CRC == 0xD643001F) {
		source = g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr);
		ram_mask = 1024 * 1024 * 64 - 1;
	}
	else
		source = g_dspInitialize.pGetARAMPointer();

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
    AFCdecodebuffer(m_AFCCoefTable, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, PB.Format);
	PB.CurAddr += 9;

	s64 TrueSamplePosition = (s64)(PB.Length - PB.RemLength) << 16;
	TrueSamplePosition += PB.CurSampleFrac;
	s64 delta = ratio >> 16;  // 0x100000000ULL;
    int sampleCount = 0;
    while (sampleCount < _Size)
    {
		int SamplePosition = TrueSamplePosition >> 16;
		_Buffer[sampleCount] = outbuf[SamplePosition & 15];

		sampleCount++;
		TrueSamplePosition += delta;

		int TargetPosition = TrueSamplePosition >> 16;

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

				AFCdecodebuffer(m_AFCCoefTable, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, PB.Format);
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
	PB.CurSampleFrac = TrueSamplePosition & 0xFFFF;
	// write back
    // NumberOfSamples = (NumberOfSamples << 4) | frac;    // missing fraction

    // i think  pTest[0x3a] and pTest[0x3b] got an update after you have decoded some samples...
    // just decrement them with the number of samples you have played
    // and increase the ARAM Offset in pTest[0x38], pTest[0x39]

    // end of block (Zelda 03b2)
}
//u32 last_remlength = 0;
// Researching what's actually inside the mysterious 0x21 case
void CUCode_Zelda::RenderVoice_Raw(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (PB.RatioInt << 16);//  + PB.RatioFrac;
	s64 ratio = (_ratio * ratioFactor) * 16; // (s64)(((_ratio / 80) << 16) * ratioFactor);

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

	u8 *source;
	u32 ram_mask = 1024 * 1024 * 16 - 1;
	if (m_CRC == 0xD643001F) {
		source = g_dspInitialize.pGetMemoryPointer(m_DMABaseAddr);
		ram_mask = 1024 * 1024 * 64 - 1;
	}
	else
		source = g_dspInitialize.pGetARAMPointer();

//restart:
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

	

	u32 prev_addr = PB.CurAddr;

	// Prefill the decode buffer.
	//AFCdecodebuffer(m_AFCCoefTable, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, PB.Format);
	const char *src = (char *)(source + (PB.CurAddr & ram_mask));
	PB.CurAddr += 9;

	s64 TrueSamplePosition = (s64)(PB.Length - PB.RemLength) << 16;
	TrueSamplePosition += PB.CurSampleFrac;
	s64 delta = ratio >> 16;  // 0x100000000ULL;
	int sampleCount = 0, realSample = 0;
	while (sampleCount < _Size)
	{
		_Buffer[sampleCount] = src[realSample] | (src[realSample + 1] << 8) | (src[realSample + 2] << 16)
								| (src[realSample + 3] << 24);
		
		//WARN_LOG(DSPHLE, "The sample: %02x", src[sampleCount]);
		
		sampleCount++;
		realSample += 4;
		TrueSamplePosition += delta;
	}

	PB.NeedsReset = 0;
	PB.CurSampleFrac = TrueSamplePosition & 0xFFFF;
}

void CUCode_Zelda::RenderAddVoice(ZeldaVoicePB &PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size)
{
	//static u16 lastLeft = 0x1FF, lastRight = 0x1FF;
	memset(m_TempBuffer, 0, _Size * sizeof(s32));

	if (PB.IsBlank)
	{
		s32 sample = (s32)(s16)PB.FixedSample;
		for (int i = 0; i < _Size; i++)
			m_TempBuffer[i] = sample;
	}
	else
	{
		// XK: Use this to disable music (GREAT for testing)
		//if(PB.SoundType == 0x0d00) {
		//	PB.NeedsReset = 0;
		//	return;
		//}
		//WARN_LOG(DSPHLE, "Fmt %04x, %04x: %04x %04x", 
		//	PB.Format, PB.SoundType, PB.Unk29, PB.Unk2a);
		/*WARN_LOG(DSPHLE, "Fmt %04x, %04x: %04x %04x %04x %04x %04x %04x %04x %04x", 
			PB.Format, PB.SoundType,
			PB.volumeLeft1, PB.volumeLeft2, PB.volumeRight1, PB.volumeRight2,
			PB.volumeUnknown1_1, PB.volumeUnknown1_2, PB.volumeUnknown2_1,
			PB.volumeUnknown2_2);*/

		switch (PB.Format)
		{
		// Synthesized sounds
		case 0x0000: // Example: Magic meter filling up in ZWW
		case 0x0003: 
			RenderSynth_RectWave(PB, m_TempBuffer, _Size);
			break;

		case 0x0001: // Example: "Denied" sound when trying to pull out a sword 
					 // indoors in ZWW
			RenderSynth_SawWave(PB, m_TempBuffer, _Size);
			break;

		case 0x0006:
			WARN_LOG(DSPHLE, "Synthesizing 0x0006 (constant sound)");
			RenderSynth_Constant(PB, m_TempBuffer, _Size);
			break;
                        
      	// These are more "synth" formats - square wave, saw wave etc.
		case 0x0002:          
			WARN_LOG(DSPHLE, "Synthesizing 0x0002");
			break;

                    
		// AFC formats
		case 0x0005:		// AFC with extra low bitrate (32:5 compression). Not yet seen.
			WARN_LOG(DSPHLE, "5 byte AFC - does it work?");
		case 0x0009:		// AFC with normal bitrate (32:9 compression).
			
		
			RenderVoice_AFC(PB, m_TempBuffer, _Size);
			break;

		case 0x0010:		// PCM16 - normal PCM 16-bit audio.
			RenderVoice_PCM16(PB, m_TempBuffer, _Size);
			//last_remlength = PB.RemLength;
			break;


		case 0x0008:   // Likely PCM8 - normal PCM 8-bit audio. Used in Mario Kart DD.
		case 0x0020:
		case 0x0021:   // Probably raw sound. Important for Zelda WW. Really need to implement - missing it causes hangs.
			WARN_LOG(DSPHLE, "Unimplemented MixAddVoice format in zelda %04x", PB.Format);

			// This is what 0x20 and 0x21 do on end of voice
			PB.RemLength = 0;
			PB.KeyOff = 1;

			// Caution: Use at your own risk. Sounds awful :)
			//RenderVoice_Raw(PB, m_TempBuffer, _Size);
			break;

		default:
			// TODO: Implement general decoder here
			ERROR_LOG(DSPHLE, "Unknown MixAddVoice format in zelda %04x", PB.Format);
			break;
		}

		// Necessary for SMG, not for Zelda. Weird.
		PB.NeedsReset = 0;
	}

// ContinueWithBlock:
	
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
				int unmixed_audio = m_TempBuffer[i];
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
			if (mix) {
				// 0ca9_RampedMultiplyAddBuffer
				for (int i = 0; i < _Size; i++)
				{
					int value = m_TempBuffer[i];

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
void CUCode_Zelda::MixAdd(short* _Buffer, int _Size)
{
	if (_Size > 256 * 1024)
		_Size = 256 * 1024;

	memset(m_LeftBuffer, 0, _Size * sizeof(s32));
	memset(m_RightBuffer, 0, _Size * sizeof(s32));

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

	if (_Buffer)
	{
		for (u32 i = 0; i < _Size; i++)
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
}