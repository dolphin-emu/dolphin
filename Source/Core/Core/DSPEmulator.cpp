// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSPEmulator.h"

#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPLLE/DSPLLE.h"

DSPEmulator *CreateDSPEmulator(bool HLE)
{
	if (HLE)
		return new DSPHLE();
	else
		return new DSPLLE();
}
