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

#ifndef __FRAME_H
#define __FRAME_H

#include "Common.h"
#include "pluginspecs_pad.h"

// Per-(video )Frame actions

namespace Frame {
	
void FrameUpdate();

bool IsAutoFiring();

void SetAutoHold(bool bEnabled, u32 keyToHold = 0);
void SetAutoFire(bool bEnabled, u32 keyOne = 0, u32 keyTwo = 0);

void SetFrameStepping(bool bEnabled);

void ModifyController(SPADStatus *PadStatus);

void SetFrameSkipping(unsigned int framesToSkip);
int FrameSkippingFactor();
void FrameSkipping();

};

#endif // __FRAME_H
