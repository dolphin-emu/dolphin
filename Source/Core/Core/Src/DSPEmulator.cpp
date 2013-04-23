// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DSPEmulator.h"

#include "HW/DSPLLE/DSPLLE.h"
#include "HW/DSPHLE/DSPHLE.h"

DSPEmulator *CreateDSPEmulator(bool HLE) 
{
	if (HLE)
	{
		return new DSPHLE();
	}
	else
	{
		return new DSPLLE();
	}
}
