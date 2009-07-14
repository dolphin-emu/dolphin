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
	s32 ratio = PB.RatioInt * 2;
	s64 pos = PB.CurSampleFrac;

	for (int i = 0; i < 0x50; i++) {
		pos += ratio;
		_Buffer[i] = pos & 0xFFFF;
	}

	PB.CurSampleFrac = pos & 0xFFFF;
}

void CUCode_Zelda::RenderSynth_Constant(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	// TODO: Header, footer
	for (int i = 0; i < _Size; i++)
		_Buffer[i++] = (s32)PB.RatioInt;
}



