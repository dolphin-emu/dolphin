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

#include <math.h>

#include "../Globals.h"
#include "UCodes.h"
#include "UCode_Zelda.h"

#include "../main.h"
#include "Mixer.h"

void CUCode_Zelda::RenderSynth_RectWave(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	float ratioFactor = 32000.0f / (float)soundStream->GetMixer()->GetSampleRate();
	u32 _ratio = (PB.RatioInt << 16);
	s64 ratio = (_ratio * ratioFactor) * 16;
	s64 TrueSamplePosition = PB.CurSampleFrac;

	// PB.Format == 0x3 -> Rectangular Wave, 0x0 -> Square Wave
	int mask = PB.Format ? 3 : 1, shift = PB.Format ? 2 : 1;
	
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

	while(i < _Size) 
	{
		s16 sample = ((pos[1] & mask) == mask) ? 0xc000 : 0x4000;

		TrueSamplePosition += (ratio >> 16);

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

	PB.CurSampleFrac = TrueSamplePosition & 0xFFFF;
}

void CUCode_Zelda::RenderSynth_SawWave(ZeldaVoicePB &PB, s32* _Buffer, int _Size) 
{
	s32 ratio = (s32)ceil((float)PB.RatioInt / 3);
	s64 pos = PB.CurSampleFrac;

	for (int i = 0; i < _Size; i++) 
	{
		pos += ratio;
		_Buffer[i] = pos & 0xFFFF;
	}

	PB.CurSampleFrac = pos & 0xFFFF;
}

void CUCode_Zelda::RenderSynth_Constant(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	// TODO: Header, footer
	for (int i = 0; i < _Size; i++)
		_Buffer[i] = (s32)PB.RatioInt;
}

// A piece of code from LLE so we can see how the wrap register affects the sound

// HORRIBLE UGLINESS, someone please fix.
// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
inline u16 ToMask(u16 a)
{
	a = a | (a >> 8);
	a = a | (a >> 4);
	a = a | (a >> 2);
	return a | (a >> 1);
}

inline s16 AddValueToReg(s16 reg, s32 value)
{
	s16 tmp = reg;
	u16 tmb = ToMask(0x003f);

	for(int i = 0; i < value; i++) {
		if ((tmp & tmb) == tmb)
			tmp ^= 0x003f;
		else
			tmp++;
	}

	return tmp;
}

void CUCode_Zelda::RenderSynth_WaveTable(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	u16 address;
	switch(PB.Format) {
	default:
	case 0x0004:
		address = 0x140;
		break;

	case 0x0007:
		address = 0x100;
		break;

	case 0x000b:
		address = 0x180;
		break;

	case 0x000c:
		address = 0x1c0;
		break;
	}

	// TODO: Resample this!
	WARN_LOG(DSPHLE, "Synthesizing the incomplete format 0x%04x", PB.Format);

	u64 ACC0 = PB.CurSampleFrac << 6;

	ACC0 &= 0xffff003fffff;

	address = AddValueToReg(address, ((ACC0 >> 16) & 0xffff));
	ACC0 &= 0xffff0000ffff;

	for(int i = 0; i < _Size; i++) 
	{
		_Buffer[i] = m_MiscTable[address];
	
		ACC0 += PB.RatioInt << 5;
		address = AddValueToReg(address, ((ACC0 >> 16) & 0xffff));

		ACC0 &= 0xffff0000ffff;
	}

	ACC0 = address << 16;
	PB.CurSampleFrac = (ACC0 >> 6) & 0xffff;
}


