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

#ifndef _UCODE_ZELDA_VOICE_H
#define _UCODE_ZELDA_VOICE_H

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

	for (int i = 0; i < _Size; i++)
	{
		s16 sample = Common::swap16(source[pos[1]]);

		_Buffer[i] = (s32)sample;

		(*(u64*)&pos) += ratio;
		if ((pos[1] + ((PB.CurAddr - PB.StartAddr) >> 1)) >= PB.Length)
		{
			PB.ReachedEnd = 1;
			_Size -= i + 1;
			goto _lRestart;
		}
	}

	PB.RemLength -= pos[1];
	PB.CurAddr += pos[1] * 2;
}

void CUCode_Zelda::MixAddVoice(ZeldaVoicePB &PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size)
{
	//float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
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
		case 0x0005:		// AFC / unknown
		case 0x0009:		// AFC / ADPCM
			// coming soon!
			return;

		case 0x0010:		// PCM16
			MixAddVoice_PCM16(PB, m_TempBuffer, _Size);
			break;
		}

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

#endif
