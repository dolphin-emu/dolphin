// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"

#include "Core/ConfigManager.h"
#include "Core/HW/GCPadEmu.h"

#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"


namespace Pad
{

static InputPlugin g_plugin("GCPadNew", _trans("Pad"), "GCPad");
InputPlugin *GetPlugin()
{
	return &g_plugin;
}

void Shutdown()
{
	std::vector<ControllerEmu*>::const_iterator
		i = g_plugin.controllers.begin(),
		e = g_plugin.controllers.end();
	for ( ; i!=e; ++i )
		delete *i;
	g_plugin.controllers.clear();

	g_controller_interface.Shutdown();
}

// if plugin isn't initialized, init and load config
void Initialize(void* const hwnd)
{
	// add 4 gcpads
	for (unsigned int i=0; i<4; ++i)
		g_plugin.controllers.push_back(new GCPad(i));

	g_controller_interface.SetHwnd(hwnd);
	g_controller_interface.Initialize();

	// load the saved controller config
	g_plugin.LoadConfig(true);
}

void GetStatus(u8 numPAD, SPADStatus* pPADStatus)
{
	memset(pPADStatus, 0, sizeof(*pPADStatus));
	pPADStatus->err = PAD_ERR_NONE;

	std::unique_lock<std::recursive_mutex> lk(g_plugin.controls_lock, std::try_to_lock);

	if (!lk.owns_lock())
	{
		// if gui has lock (messing with controls), skip this input cycle
		// center axes and return
		memset(&pPADStatus->stickX, 0x80, 4);
		return;
	}

	// if we are on the next input cycle, update output and input
	// if we can get a lock
	static int last_numPAD = 4;
	if (numPAD <= last_numPAD)
	{
		g_controller_interface.UpdateOutput();
		g_controller_interface.UpdateInput();
	}
	last_numPAD = numPAD;

	// get input
	((GCPad*)g_plugin.controllers[numPAD])->GetInput(pPADStatus);
}

// __________________________________________________________________________________________________
// Function: Rumble
// Purpose:  Pad rumble!
// input:    numPad    - Which pad to rumble.
//           uType     - Command type (Stop=0, Rumble=1, Stop Hard=2).
//           uStrength - Strength of the Rumble
// output:   none
//
void Rumble(u8 numPAD, unsigned int uType, unsigned int uStrength)
{
	std::unique_lock<std::recursive_mutex> lk(g_plugin.controls_lock, std::try_to_lock);

	if (lk.owns_lock())
	{
		// TODO: this has potential to not stop rumble if user is messing with GUI at the perfect time
		// set rumble
		if (1 == uType && uStrength > 2)
		{
			((GCPad*)g_plugin.controllers[ numPAD ])->SetOutput(255);
		}
		else
		{
			((GCPad*)g_plugin.controllers[ numPAD ])->SetOutput(0);
		}
	}
}

// __________________________________________________________________________________________________
// Function: Motor
// Purpose:  For devices with constant Force feedback
// input:    numPAD    - The pad to operate on
//           uType     - 06 = Motor On, 04 = Motor Off
//           uStrength - 00 = Left Strong, 127 = Left Weak, 128 = Right Weak, 255 = Right Strong
// output:   none
//
void Motor(u8 numPAD, unsigned int uType, unsigned int uStrength)
{
	std::unique_lock<std::recursive_mutex> lk(g_plugin.controls_lock, std::try_to_lock);

	if (lk.owns_lock())
	{
		// TODO: this has potential to not stop rumble if user is messing with GUI at the perfect time
		// set rumble
		if (uType == 6)
		{
			((GCPad*)g_plugin.controllers[ numPAD ])->SetMotor(uStrength);
		}
	}
}

bool GetMicButton(u8 pad)
{

	std::unique_lock<std::recursive_mutex> lk(g_plugin.controls_lock, std::try_to_lock);

	if (!lk.owns_lock())
		return false;

	return ((GCPad*)g_plugin.controllers[pad])->GetMicButton();
}

}
