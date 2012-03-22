// Copyright (C) 2010 Dolphin Project.

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

#include "Common.h"
#include "GCPadStatus.h"

#include "ControllerInterface/ControllerInterface.h"
#include "GCPadEmu.h"

#include "../../InputCommon/Src/InputConfig.h"

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
	g_plugin.LoadConfig();
}

void GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	memset(_pPADStatus, 0, sizeof(*_pPADStatus));
	_pPADStatus->err = PAD_ERR_NONE;

	std::unique_lock<std::recursive_mutex> lk(g_plugin.controls_lock, std::try_to_lock);

	if (!lk.owns_lock())
	{
		// if gui has lock (messing with controls), skip this input cycle
		// center axes and return
		memset(&_pPADStatus->stickX, 0x80, 4);
		return;
	}

	// if we are on the next input cycle, update output and input
	// if we can get a lock
	static int _last_numPAD = 4;
	if (_numPAD <= _last_numPAD)
	{
		g_controller_interface.UpdateOutput();
		g_controller_interface.UpdateInput();
	}
	_last_numPAD = _numPAD;
	
	// get input
	((GCPad*)g_plugin.controllers[_numPAD])->GetInput(_pPADStatus);
}

// __________________________________________________________________________________________________
// Function: PAD_Rumble
// Purpose:  Pad rumble!
// input:	 PAD number, Command type (Stop=0, Rumble=1, Stop Hard=2) and strength of Rumble
// output:   none
//
void Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	std::unique_lock<std::recursive_mutex> lk(g_plugin.controls_lock, std::try_to_lock);

	if (lk.owns_lock())
	{
		// TODO: this has potential to not stop rumble if user is messing with GUI at the perfect time
		// set rumble
		((GCPad*)g_plugin.controllers[ _numPAD ])->SetOutput( 1 == _uType && _uStrength > 2 );
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
