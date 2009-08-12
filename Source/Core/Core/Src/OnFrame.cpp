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

#include "OnFrame.h"

#include "Core.h"
#include "PluginManager.h"
#include "Thread.h"

Common::CriticalSection cs_frameSkip;

namespace Frame {

bool g_bFrameStep = false;
bool g_bAutoFire = false;
u32 g_autoFirstKey = 0, g_autoSecondKey = 0;
bool g_bFirstKey = true;

int g_framesToSkip = 0, g_frameSkipCounter = 0;

void FrameUpdate() {


	if (g_bFrameStep)
		Core::SetState(Core::CORE_PAUSE);

	FrameSkipping();

	if (g_bAutoFire)
		g_bFirstKey = !g_bFirstKey;
	
}

void SetFrameSkipping(unsigned int framesToSkip) {
	cs_frameSkip.Enter();

	g_framesToSkip = (int)framesToSkip;
	g_frameSkipCounter = 0;

	cs_frameSkip.Leave();
}

int FrameSkippingFactor() {
	return g_framesToSkip;
}

void SetAutoHold(bool bEnabled, u32 keyToHold)
{
	g_bAutoFire = bEnabled;
	if (bEnabled)
		g_autoFirstKey = g_autoSecondKey = keyToHold;
	else
		g_autoFirstKey = g_autoSecondKey = 0;
}

void SetAutoFire(bool bEnabled, u32 keyOne, u32 keyTwo)
{
	g_bAutoFire = bEnabled;
	if (bEnabled) {
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

void ModifyController(SPADStatus *PadStatus)
{
	u32 keyToPress = (g_bFirstKey) ? g_autoFirstKey : g_autoSecondKey;
	
	if (!keyToPress)
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

void FrameSkipping()
{
	cs_frameSkip.Enter();

	g_frameSkipCounter++;
	if (g_frameSkipCounter > g_framesToSkip || Core::report_slow(g_frameSkipCounter) == false)
		g_frameSkipCounter = 0;

	CPluginManager::GetInstance().GetVideo()->Video_SetRendering(!g_frameSkipCounter);

	cs_frameSkip.Leave();
}

};
