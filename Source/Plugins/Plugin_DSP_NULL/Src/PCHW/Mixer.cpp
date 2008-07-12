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

#include "../Config.h"
#include "../Globals.h"
#include "../DSPHandler.h"
#include "Mixer.h"

volatile bool mixer_HLEready = false;

void Mixer(short* buffer, int numSamples, int bits, int rate, int channels)
{
	// silence
	memset(buffer, 0, numSamples * 2 * sizeof(short));

	// first get th DTK Music
	if (g_Config.m_EnableDTKMusic)
	{
		g_dspInitialize.pGetAudioStreaming(buffer, numSamples);
	}

	//if this was called directly from the HLE, and not by timeout
	if (g_Config.m_EnableHLEAudio && (mixer_HLEready || g_Config.m_AntiGap))
	{
		IUCode* pUCode = CDSPHandler::GetInstance().GetUCode();

		if (pUCode != NULL)
		{
			pUCode->MixAdd(buffer, numSamples);
		}
	}
}


