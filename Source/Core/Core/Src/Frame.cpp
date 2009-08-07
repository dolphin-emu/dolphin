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

#include "Core.h"
#include "PluginManager.h"

namespace Frame {

bool g_bFrameStep = false;
bool g_bAutoFire = false;
u32 g_autoFirstKey = 0, g_autoSecondKey = 0;
bool g_bFirstKey = true;

void FrameUpdate() {
	if(g_bFrameStep)
		Core::SetState(Core::CORE_PAUSE);

	if(g_bAutoFire)
		g_bFirstKey = !g_bFirstKey;
}

void SetAutoHold(bool bEnabled, u32 keyToHold) {
	g_bAutoFire = bEnabled;
	if(bEnabled)
		g_autoFirstKey = g_autoSecondKey = keyToHold;
	else
		g_autoFirstKey = g_autoSecondKey = 0;
}

void SetAutoFire(bool bEnabled, u32 keyOne, u32 keyTwo) {
	g_bAutoFire = bEnabled;
	if(bEnabled) {
		g_autoFirstKey = keyOne;
		g_autoSecondKey = keyTwo;
	} else
		g_autoFirstKey = g_autoSecondKey = 0;

	g_bFirstKey = true;
}

bool IsAutoFiring() {
	return g_bAutoFire;
}

void SetFrameStepping(bool bEnabled) {
	g_bFrameStep = bEnabled;
}


void ModifyController(SPADStatus *PadStatus) {
	u32 keyToPress = (g_bFirstKey) ? g_autoFirstKey : g_autoSecondKey;
	
	if(!keyToPress)
		return;
	
	PadStatus->button |= keyToPress;

	switch(keyToPress) {
		default:
			return;

		case PAD_BUTTON_A:
			PadStatus->analogA = 255;
			break;
		case PAD_BUTTON_B:
			PadStatus->analogB = 255;
			break;
		
		case PAD_TRIGGER_L:
			PadStatus->triggerLeft = 255;
			break;
		case PAD_TRIGGER_R:
			PadStatus->triggerRight = 255;
			break;
	}

}

};
