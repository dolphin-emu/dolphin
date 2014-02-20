// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/HLEMixer.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

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

