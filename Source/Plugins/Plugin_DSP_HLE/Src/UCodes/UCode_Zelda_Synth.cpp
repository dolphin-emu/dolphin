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

void CUCode_Zelda::MixAddSynth_Waveform(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	int mask = PB.Format ? 3 : 1;
        
	for (int i = 0; i < _Size; i++)
	{
		s16 sample = (i & mask) ? 0xc000 : 0x4000;

		_Buffer[i++] = (s32)sample;
	}
}


void CUCode_Zelda::MixAddSynth_Constant(ZeldaVoicePB &PB, s32* _Buffer, int _Size)
{
	for (int i = 0; i < _Size; i++)
            _Buffer[i++] = (s32)PB.RatioInt;
}



