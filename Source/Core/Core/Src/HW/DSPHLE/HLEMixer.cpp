// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DSPHLE.h"
#include "HLEMixer.h"
#include "UCodes/UCodes.h"

void HLEMixer::Premix(short *samples, unsigned int numSamples)
{
	// if this was called directly from the HLE
	if (IsHLEReady())
	{
		IUCode *pUCode = m_DSPHLE->GetUCode();
		if (pUCode && samples)
			pUCode->MixAdd(samples, numSamples);
	}
}

