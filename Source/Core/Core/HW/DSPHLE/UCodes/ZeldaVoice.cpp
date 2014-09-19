// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sstream>

#include "Common/CommonFuncs.h"
#include "Common/MathUtil.h"

#include "Core/HW/DSP.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/HW/DSPHLE/UCodes/Zelda.h"

void ZeldaUCode::ReadVoicePB(u32 _Addr, ZeldaVoicePB& PB)
{
	u16 *memory = (u16*)Memory::GetPointer(_Addr);

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

void ZeldaUCode::WritebackVoicePB(u32 _Addr, ZeldaVoicePB& PB)
{
	u16 *memory = (u16*)Memory::GetPointer(_Addr);

	// Word swap all 32-bit variables.
	PB.RestartPos = (PB.RestartPos << 16) | (PB.RestartPos >> 16);
	PB.CurAddr = (PB.CurAddr << 16) | (PB.CurAddr >> 16);
	PB.RemLength = (PB.RemLength << 16) | (PB.RemLength >> 16);

	// Perform byteswap
	// Only the first 0x100 bytes are written back
	for (int i = 0; i < (0x100 / 2); i++)
		memory[i] = Common::swap16(((u16*)&PB)[i]);
}

int ZeldaUCode::ConvertRatio(int pb_ratio)
{
	return pb_ratio * 16;
}

int ZeldaUCode::SizeForResampling(ZeldaVoicePB &PB, int size)
{
	// This is the little calculation at the start of every sample decoder
	// in the ucode.
	return (PB.CurSampleFrac + size * ConvertRatio(PB.RatioInt)) >> 16;
}

// Simple resampler, linear interpolation.
// Any future state should be stored in PB.raw[0x3c to 0x3f].
// In must point 4 samples into a buffer.
void ZeldaUCode::Resample(ZeldaVoicePB &PB, int size, s16 *in, s32 *out, bool do_resample)
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
	int in_size = SizeForResampling(PB, size);

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

static void UpdateSampleCounters10(ZeldaVoicePB &PB)
{
	PB.RemLength = PB.Length - PB.RestartPos;
	PB.CurAddr = PB.StartAddr + (PB.RestartPos << 1);
	PB.ReachedEnd = 0;
}

void ZeldaUCode::RenderVoice_PCM16(ZeldaVoicePB &PB, s16 *_Buffer, int _Size)
{
	int _RealSize = SizeForResampling(PB, _Size);
	u32 rem_samples = _RealSize;
	if (PB.KeyOff)
		goto clear_buffer;
	if (PB.NeedsReset)
	{
		UpdateSampleCounters10(PB);
		for (int i = 0; i < 4; i++)
			PB.ResamplerOldData[i] = 0;  // Doesn't belong here, but dunno where to do it.
	}
	if (PB.ReachedEnd)
	{
		PB.ReachedEnd = 0;
reached_end:
		if (!PB.RepeatMode)
		{
			// One shot - play zeros the rest of the buffer.
clear_buffer:
			for (u32 i = 0; i < rem_samples; i++)
				*_Buffer++ = 0;
			PB.KeyOff = 1;
			return;
		}
		else
		{
			PB.RestartPos = PB.LoopStartPos;
			UpdateSampleCounters10(PB);
		}
	}
	// SetupAccelerator
	const s16 *read_ptr = (s16*)GetARAMPointer(PB.CurAddr);
	if (PB.RemLength < (u32)rem_samples)
	{
		// finish-up loop
		for (u32 i = 0; i < PB.RemLength; i++)
			*_Buffer++ = Common::swap16(*read_ptr++);
		rem_samples -= PB.RemLength;
		goto reached_end;
	}
	// main render loop
	for (u32 i = 0; i < rem_samples; i++)
		*_Buffer++ = Common::swap16(*read_ptr++);

	PB.RemLength -= rem_samples;
	if (PB.RemLength == 0)
		PB.ReachedEnd = 1;
	PB.CurAddr += rem_samples << 1;
}

static void UpdateSampleCounters8(ZeldaVoicePB &PB)
{
	PB.RemLength = PB.Length - PB.RestartPos;
	PB.CurAddr = PB.StartAddr + PB.RestartPos;
	PB.ReachedEnd = 0;
}

void ZeldaUCode::RenderVoice_PCM8(ZeldaVoicePB &PB, s16 *_Buffer, int _Size)
{
	int _RealSize = SizeForResampling(PB, _Size);
	u32 rem_samples = _RealSize;
	if (PB.KeyOff)
		goto clear_buffer;
	if (PB.NeedsReset)
	{
		UpdateSampleCounters8(PB);
		for (int i = 0; i < 4; i++)
			PB.ResamplerOldData[i] = 0;  // Doesn't belong here, but dunno where to do it.
	}
	if (PB.ReachedEnd)
	{
reached_end:
		PB.ReachedEnd = 0;
		if (!PB.RepeatMode)
		{
			// One shot - play zeros the rest of the buffer.
clear_buffer:
			for (u32 i = 0; i < rem_samples; i++)
				*_Buffer++ = 0;
			PB.KeyOff = 1;
			return;
		}
		else
		{
			PB.RestartPos = PB.LoopStartPos;
			UpdateSampleCounters8(PB);
		}
	}

	// SetupAccelerator
	const s8 *read_ptr = (s8*)GetARAMPointer(PB.CurAddr);
	if (PB.RemLength < (u32)rem_samples)
	{
		// finish-up loop
		for (u32 i = 0; i < PB.RemLength; i++)
			*_Buffer++ = (s8)(*read_ptr++) << 8;
		rem_samples -= PB.RemLength;
		goto reached_end;
	}
	// main render loop
	for (u32 i = 0; i < rem_samples; i++)
		*_Buffer++ = (s8)(*read_ptr++) << 8;

	PB.RemLength -= rem_samples;
	if (PB.RemLength == 0)
		PB.ReachedEnd = 1;
	PB.CurAddr += rem_samples;
}

template <typename T>
void PrintObject(const T &Obj)
{
	std::stringstream ss;
	u8 *o = (u8 *)&Obj;

	// If this miscompiles, adjust the size of
	// ZeldaVoicePB to 0x180 bytes (0xc0 shorts).
	static_assert(sizeof(ZeldaVoicePB) == 0x180, "ZeldaVoicePB incorrectly defined.");

	ss << std::hex;
	for (size_t i = 0; i < sizeof(T); i++)
	{
		if ((i & 1) == 0)
			ss << ' ';
		ss.width(2);
		ss.fill('0');
		ss << Common::swap16(o[i]);
	}

	DEBUG_LOG(DSPHLE, "AFC PB:%s", ss.str().c_str());
}

void ZeldaUCode::RenderVoice_AFC(ZeldaVoicePB &PB, s16 *_Buffer, int _Size)
{
	// TODO: Compare mono, stereo and surround samples
#if defined DEBUG || defined DEBUGFAST
	PrintObject(PB);
#endif

	int _RealSize = SizeForResampling(PB, _Size);

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
	{
		for (int i = 0; i < _RealSize; i++)
			*_Buffer++ = 0;
		return;
	}

	// Round upwards how many samples we need to copy, 0759
	// u32 frac = NumberOfSamples & 0xF;
	// NumberOfSamples = (NumberOfSamples + 0xf) >> 4;   // i think the lower 4 are the fraction

	const u8 *source;
	u32 ram_mask = 1024 * 1024 * 16 - 1;
	if (IsDMAVersion())
	{
		source = Memory::GetPointer(m_dma_base_addr);
		ram_mask = 1024 * 1024 * 64 - 1;
	}
	else
	{
		source = DSP::GetARAMPtr();
	}

	int sampleCount = 0;  // must be above restart.

restart:
	if (PB.ReachedEnd)
	{
		PB.ReachedEnd = 0;

		if ((PB.RepeatMode == 0) || (PB.StopOnSilence != 0))
		{
			PB.KeyOff = 1;
			PB.RemLength = 0;
			PB.CurAddr = PB.StartAddr + PB.RestartPos + PB.Length;

			while (sampleCount < _RealSize)
				_Buffer[sampleCount++] = 0;
			return;
		}
		else
		{
			//AFC looping
			// The loop start pos is incorrect? (Fixed?), so samples will loop a bit wrong.
			// this fixes the intro music in ZTP.
			PB.RestartPos = PB.LoopStartPos;
			PB.RemLength = PB.Length - PB.RestartPos;
			// see DSP_UC_Zelda.txt line 2817
			PB.CurAddr = ((((((PB.LoopStartPos >> 4) & 0xffff0000)*PB.Format)<<16)+
				(((PB.LoopStartPos >> 4) & 0xffff)*PB.Format))+PB.StartAddr) & 0xffffffff;

			// Hmm, this shouldn't be reversed .. or should it? Is it different between versions of the ucode?
			// -> it has to be reversed in ZTP, otherwise intro music is broken...
			PB.YN1 = PB.LoopYN2;
			PB.YN2 = PB.LoopYN1;
		}
	}

	short outbuf[16] = {0};
	u16 prev_yn1 = PB.YN1;
	u16 prev_yn2 = PB.YN2;
	u32 prev_addr = PB.CurAddr;

	// Prefill the decode buffer.
	AFCdecodebuffer(m_afc_coef_table, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, PB.Format);
	PB.CurAddr += PB.Format;  // 9 or 5

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
		if ((SamplePosition & 15) == 0)
		{
			prev_yn1 = PB.YN1;
			prev_yn2 = PB.YN2;
			prev_addr = PB.CurAddr;

			AFCdecodebuffer(m_afc_coef_table, (char*)(source + (PB.CurAddr & ram_mask)), outbuf, (short*)&PB.YN2, (short*)&PB.YN1, PB.Format);
			PB.CurAddr += PB.Format;  // 9 or 5
		}
	}

	// Here we should back off to the previous addr/yn1/yn2, since we didn't consume the full last block.
	// We'll re-decode it the next time around.
	PB.YN2 = prev_yn2;
	PB.YN1 = prev_yn1;
	PB.CurAddr = prev_addr;

	PB.NeedsReset = 0;
	// PB.CurBlock = 0x10 - (PB.LoopStartPos & 0xf);
	// write back
	// NumberOfSamples = (NumberOfSamples << 4) | frac;    // missing fraction

	// i think  pTest[0x3a] and pTest[0x3b] got an update after you have decoded some samples...
	// just decrement them with the number of samples you have played
	// and increase the ARAM Offset in pTest[0x38], pTest[0x39]

	// end of block (Zelda 03b2)
}

void Decoder21_ReadAudio(ZeldaVoicePB &PB, int size, s16 *_Buffer);

// Researching what's actually inside the mysterious 0x21 case
// 0x21 seems to really just be reading raw 16-bit audio from RAM (not ARAM).
// The rules seem to be quite different, though.
// It's used for streaming, not for one-shot or looped sample playback.
void ZeldaUCode::RenderVoice_Raw(ZeldaVoicePB &PB, s16 *_Buffer, int _Size)
{
	// Decoder0x21 starts here.
	u32 _RealSize = SizeForResampling(PB, _Size);

	// Decoder0x21Core starts here.
	u32 AX0 = _RealSize;

	// ERROR_LOG(DSPHLE, "0x21 volume mode: %i , stop: %i ", PB.VolumeMode, PB.StopOnSilence);

	// The PB.StopOnSilence check is a hack, we should check the buffers and enter this
	// only when the buffer is completely 0 (i.e. when the music has finished fading out)
	if (PB.StopOnSilence || PB.RemLength < (u32)_RealSize)
	{
		WARN_LOG(DSPHLE, "Raw: END");
		// Let's ignore this entire case since it doesn't seem to happen
		// in Zelda, since Length is set to 0xF0000000
		// blah
		// blah
		// readaudio
		// blah
		PB.RemLength = 0;
		PB.KeyOff = 1;
	}

	PB.RemLength -= _RealSize;

	u64 ACC0 = (u32)(PB.raw[0x8a ^ 1] << 16);  // 0x8a    0ad5, yes it loads a, not b
	u64 ACC1 = (u32)(PB.raw[0x34 ^ 1] << 16);  // 0x34

	// ERROR_LOG(DSPHLE, "%08x %08x", (u32)ACC0, (u32)ACC1);

	ACC0 -= ACC1;

	PB.Unk36[0] = (u16)(ACC0 >> 16);

	ACC0 -= AX0 << 16;

	if ((s64)ACC0 < 0)
	{
		// ERROR_LOG(DSPHLE, "Raw loop: ReadAudio size = %04x 34:%04x %08x", PB.Unk36[0], PB.raw[0x34 ^ 1], (int)ACC0);
		Decoder21_ReadAudio(PB, PB.Unk36[0], _Buffer);

		ACC0 = -(s64)ACC0;
		_Buffer += PB.Unk36[0];

		PB.raw[0x34 ^ 1] = 0;

		PB.StartAddr = PB.LoopStartPos;

		Decoder21_ReadAudio(PB, (int)(ACC0 >> 16), _Buffer);
		return;
	}

	Decoder21_ReadAudio(PB, _RealSize, _Buffer);
}

void Decoder21_ReadAudio(ZeldaVoicePB &PB, int size, s16 *_Buffer)
{
	// 0af6
	if (!size)
		return;

#if 0
	// 0afa
	u32 AX1 = (PB.RestartPos >> 16) & 1;  // PB.raw[0x34], except that it's part of a dword
	// 0b00 - Eh, WTF.
	u32 ACC0 = PB.StartAddr + ((PB.RestartPos >> 16) << 1) - 2*AX1;
	u32 ACC1 = (size << 16) + 0x20000;
	// All this trickery, and more, seems to be to align the DMA, which
	// we really don't care about. So let's skip it. See the #else.

#else
	// ERROR_LOG(DSPHLE, "ReadAudio: %08x %08x", PB.StartAddr, PB.raw[0x34 ^ 1]);
	u32 ACC0 = PB.StartAddr + (PB.raw[0x34 ^ 1] << 1);
	u32 ACC1 = (size << 16);
#endif
	// ACC0 is the address
	// ACC1 is the read size

	const u32 ram_mask = 0x1FFFFFF;
	const u8 *source = Memory::GetPointer(0x80000000);
	const u16 *src = (u16 *)(source + (ACC0 & ram_mask));

	for (u32 i = 0; i < (ACC1 >> 16); i++)
	{
		_Buffer[i] = Common::swap16(src[i]);
	}

	PB.raw[0x34 ^ 1] += size;
}


void ZeldaUCode::RenderAddVoice(ZeldaVoicePB &PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size)
{
	if (PB.IsBlank)
	{
		s32 sample = (s32)(s16)PB.FixedSample;
		for (int i = 0; i < _Size; i++)
			m_voice_buffer[i] = sample;

		goto ContinueWithBlock;  // Yes, a goto. Yes, it's evil, but it makes the flow look much more like the DSP code.
	}

	// XK: Use this to disable MIDI music (GREAT for testing). Also kills some sound FX.
	//if (PB.SoundType == 0x0d00)
	//{
	//    PB.NeedsReset = 0;
	//    return;
	//}

	// The Resample calls actually don't resample yet.

	// ResampleBuffer corresponds to 0x0580 in ZWW ucode.
	// VoiceBuffer corresponds to 0x0520.

	// First jump table at ZWW: 2a6
	switch (PB.Format)
	{
	case 0x0005: // AFC with extra low bitrate (32:5 compression).
	case 0x0009: // AFC with normal bitrate (32:9 compression).
		RenderVoice_AFC(PB, m_resample_buffer + 4, _Size);
		Resample(PB, _Size, m_resample_buffer + 4, m_voice_buffer, true);
		break;

	case 0x0008: // PCM8 - normal PCM 8-bit audio. Used in Mario Kart DD + very little in Zelda WW.
		RenderVoice_PCM8(PB, m_resample_buffer + 4, _Size);
		Resample(PB, _Size, m_resample_buffer + 4, m_voice_buffer, true);
		break;

	case 0x0010: // PCM16 - normal PCM 16-bit audio.
		RenderVoice_PCM16(PB, m_resample_buffer + 4, _Size);
		Resample(PB, _Size, m_resample_buffer + 4, m_voice_buffer, true);
		break;

	case 0x0020:
		// Normally, this shouldn't resample, it should just decode directly
		// to the output buffer. However, (if we ever see this sound type), we'll
		// have to resample anyway since we're running at a different sample rate.

		RenderVoice_Raw(PB, m_resample_buffer + 4, _Size);
		Resample(PB, _Size, m_resample_buffer + 4, m_voice_buffer, true);
		break;

	case 0x0021:
		// Raw sound from RAM. Important for Zelda WW. Cutscenes use the music
		// to let the game know they ended
		RenderVoice_Raw(PB, m_resample_buffer + 4, _Size);
		Resample(PB, _Size, m_resample_buffer + 4, m_voice_buffer, true);
		break;

	default:
		// Second jump table
		// TODO: Cases to find examples of:
		//       -0x0002
		//       -0x0003
		//       -0x0006
		//       -0x000a
		switch (PB.Format)
		{
		// Synthesized sounds
		case 0x0003: WARN_LOG(DSPHLE, "PB Format 0x03 used!");
		case 0x0000: // Example: Magic meter filling up in ZWW
			RenderSynth_RectWave(PB, m_voice_buffer, _Size);
			break;

		case 0x0001: // Example: "Denied" sound when trying to pull out a sword indoors in ZWW
			RenderSynth_SawWave(PB, m_voice_buffer, _Size);
			break;

		case 0x0006:
			WARN_LOG(DSPHLE, "Synthesizing 0x0006 (constant sound)");
			RenderSynth_Constant(PB, m_voice_buffer, _Size);
			break;

		// These are more "synth" formats - square wave, saw wave etc.
		case 0x0002:
			WARN_LOG(DSPHLE, "PB Format 0x02 used!");
			break;

		case 0x0004: // Example: Big Pikmin onion mothership landing/building a bridge in Pikmin
		case 0x0007: // Example: "success" SFX in Pikmin 1, Pikmin 2 in a cave, not sure what sound it is.
		case 0x000b: // Example: SFX in area selection menu in Pikmin
		case 0x000c: // Example: beam of death/yellow force-field in Temple of the Gods, ZWW
			RenderSynth_WaveTable(PB, m_voice_buffer, _Size);
			break;

		default:
			// TODO: Implement general decoder here
			memset(m_voice_buffer, 0, _Size * sizeof(s32));
			ERROR_LOG(DSPHLE, "Unknown MixAddVoice format in zelda %04x", PB.Format);
			break;
		}
	}

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
		// TODO?
	}

	// Apply volume. There are two different modes.
	if (PB.VolumeMode != 0)
	{
		// Complex volume mode. Let's see what we can do.
		if (PB.StopOnSilence)
		{
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
		AX0L = m_misc_table[0x200 + AX0L];
		AX0H = m_misc_table[0x200 + AX0H];
		AX1L = m_misc_table[0x200 + AX1L];
		AX1H = m_misc_table[0x200 + AX1H];

		short b00[20];
		b00[0] = AX1L * AX1H >> 16;
		b00[1] = AX0L * AX1H >> 16;
		b00[2] = AX0H * AX1L >> 16;
		b00[3] = AX0L * AX0H >> 16;

		for (int i = 0; i < 4; i++)
		{
			b00[i + 4] = (s16)b00[i] * (s16)PB.raw[0x2a] >> 16;
		}

		int prod = ((s16)PB.raw[0x2a] * (s16)PB.raw[0x29] * 2) >> 16;
		for (int i = 0; i < 4; i++)
		{
			b00[i + 8] = (s16)b00[i + 4] * prod;
		}

		// ZWW 0d34

		int diff = (s16)PB.raw[0x2b] - (s16)PB.raw[0x2a];
		PB.raw[0x2a] = PB.raw[0x2b];

		for (int i = 0; i < 4; i++)
		{
			b00[i + 0xc] = (unsigned short)b00[i] * diff >> 16;
		}

		for (int i = 0; i < 4; i++)
		{
			b00[i + 0x10] = (s16)b00[i + 0xc] * PB.raw[0x29];
		}

		for (int count = 0; count < 8; count++)
		{
			// The 8 buffers to mix to: 0d00, 0d60, 0f40 0ca0 0e80 0ee0 0c00 0c50
			// We just mix to the first two and call it stereo :p
			int value = b00[0x4 + count];
			//int delta = b00[0xC + count] << 11; // Unused?

			int ramp = value << 16;
			for (int i = 0; i < _Size; i++)
			{
				int unmixed_audio = m_voice_buffer[i];
				switch (count)
				{
				case 0: _LeftBuffer[i] += (u64)unmixed_audio * ramp >> 29; break;
				case 1: _RightBuffer[i] += (u64)unmixed_audio * ramp >> 29; break;
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

			if (sum == 0)
			{
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
					int value = m_voice_buffer[i];

					// TODO - add to buffer specified by dest_buffer_address
					switch (count)
					{
						// These really should be 32.
						case 0: _LeftBuffer[i] += (u64)value * ramp >> 29; break;
						case 1: _RightBuffer[i] += (u64)value * ramp >> 29; break;
					}

					if (((i & 1) == 0) && i < 64)
					{
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
	// 03b2, this is the reason of using PB.NeedsReset. Seems to be necessary for SMG, and maybe other games.
	if (PB.IsBlank == 0)
	{
		PB.NeedsReset = 0;
	}
}

void ZeldaUCode::MixAudio()
{
	const int BufferSamples = 5 * 16;

	// Final mix buffers
	memset(m_left_buffer, 0, BufferSamples * sizeof(s32));
	memset(m_right_buffer, 0, BufferSamples * sizeof(s32));

	// For each PB...
	for (u32 i = 0; i < m_num_voices; i++)
	{
		if (!IsLightVersion())
		{
			u32 flags = m_sync_flags[(i >> 4) & 0xF];
			if (!(flags & 1 << (15 - (i & 0xF))))
				continue;
		}

		ZeldaVoicePB pb;
		ReadVoicePB(m_voice_pbs_addr + (i * 0x180), pb);

		if (pb.Status == 0)
			continue;
		if (pb.KeyOff != 0)
			continue;

		RenderAddVoice(pb, m_left_buffer, m_right_buffer, BufferSamples);
		WritebackVoicePB(m_voice_pbs_addr + (i * 0x180), pb);
	}

	// Post processing, final conversion.
	s16* left_buffer = (s16*)HLEMemory_Get_Pointer(m_left_buffers_addr);
	s16* right_buffer = (s16*)HLEMemory_Get_Pointer(m_right_buffers_addr);
	left_buffer += m_current_buffer * BufferSamples;
	right_buffer += m_current_buffer * BufferSamples;
	for (int i = 0; i < BufferSamples; i++)
	{
		s32 left = m_left_buffer[i];
		s32 right = m_right_buffer[i];

		MathUtil::Clamp(&left, -32768, 32767);
		left_buffer[i] = Common::swap16((short)left);

		MathUtil::Clamp(&right, -32768, 32767);
		right_buffer[i] = Common::swap16((short)right);
	}
}
