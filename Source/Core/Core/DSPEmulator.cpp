// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Core/DSPEmulator.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPLLE/DSPLLE.h"

std::unique_ptr<DSPEmulator> CreateDSPEmulator(bool hle)
{
	if (hle)
		return std::make_unique<DSPHLE>();

	return std::make_unique<DSPLLE>();
}
