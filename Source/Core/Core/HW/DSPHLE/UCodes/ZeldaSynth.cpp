// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/HW/DSPHLE/UCodes/Zelda.h"

void ZeldaUCode::RenderSynth_RectWave(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	s64 ratio = ((s64)PB.RatioInt << 16) * 16;
	s64 TrueSamplePosition = PB.CurSampleFrac;

	// PB.Format == 0x3 -> Rectangular Wave, 0x0 -> Square Wave
	unsigned int mask = PB.Format ? 3 : 1;
	// int shift = PB.Format ? 2 : 1; // Unused?

	u32 pos[2] = {0, 0};
	int i = 0;

	if (PB.KeyOff != 0)
		return;

	if (PB.NeedsReset)
	{
		PB.RemLength = PB.Length - PB.RestartPos;
		PB.CurAddr = PB.StartAddr + (PB.RestartPos << 1);
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
			PB.CurAddr = PB.StartAddr + (PB.RestartPos << 1);
			pos[1] = 0; pos[0] = 0;
		}
	}

	while (i < _Size)
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
	{
		PB.RemLength -= pos[1];
	}

	PB.CurSampleFrac = TrueSamplePosition & 0xFFFF;
}

void ZeldaUCode::RenderSynth_SawWave(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
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

void ZeldaUCode::RenderSynth_Constant(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	// TODO: Header, footer
	for (int i = 0; i < _Size; i++)
		_Buffer[i] = (s32)PB.RatioInt;
}

// A piece of code from LLE so we can see how the wrap register affects the sound

inline u16 AddValueToReg(u32 ar, s32 ix)
{
	u32 wr = 0x3f;
	u32 mx = (wr | 1) << 1;
	u32 nar = ar + ix;
	u32 dar = (nar ^ ar ^ ix) & mx;

	if (ix >= 0)
	{
		if (dar > wr) //overflow
			nar -= wr + 1;
	}
	else
	{
		if ((((nar + wr + 1) ^ nar) & dar) <= wr) //underflow or below min for mask
			nar += wr + 1;
	}
	return nar;
}

void ZeldaUCode::RenderSynth_WaveTable(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	u16 address;

	switch (PB.Format)
	{
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
	INFO_LOG(DSPHLE, "Synthesizing the incomplete format 0x%04x", PB.Format);

	u64 ACC0 = PB.CurSampleFrac << 6;

	ACC0 &= 0xffff003fffffULL;

	address = AddValueToReg(address, ((ACC0 >> 16) & 0xffff));
	ACC0 &= 0xffff0000ffffULL;

	for (int i = 0; i < 0x50; i++)
	{
		_Buffer[i] = m_misc_table[address];

		ACC0 += PB.RatioInt << 5;
		address = AddValueToReg(address, ((ACC0 >> 16) & 0xffff));

		ACC0 &= 0xffff0000ffffULL;
	}

	ACC0 += address << 16;
	PB.CurSampleFrac = (ACC0 >> 6) & 0xffff;
}


