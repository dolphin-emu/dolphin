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

static InputPlugin g_plugin("GCPadNew", "Pad", "GCPad");

InputPlugin *PAD_GetPlugin() {
	return &g_plugin;
}

void GCPad_Deinit()
{
	if ( g_plugin.controller_interface.IsInit() )
	{
		std::vector<ControllerEmu*>::const_iterator
			i = g_plugin.controllers.begin(),
			e = g_plugin.controllers.end();
		for ( ; i!=e; ++i )
			delete *i;
		g_plugin.controllers.clear();

		g_plugin.controller_interface.DeInit();
	}
}

// if plugin isn't initialized, init and load config
void GCPad_Init( void* const hwnd )
{
	// i realize i am checking IsInit() twice, just too lazy to change it
	if ( false == g_plugin.controller_interface.IsInit() )
	{
		// add 4 gcpads
		for (unsigned int i=0; i<4; ++i)
			g_plugin.controllers.push_back(new GCPad(i));
		
		// needed for Xlib
		g_plugin.controller_interface.SetHwnd(hwnd);
		g_plugin.controller_interface.Init();

		// load the saved controller config
		g_plugin.LoadConfig();
	}
}

void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	memset(_pPADStatus, 0, sizeof(*_pPADStatus));
	_pPADStatus->err = PAD_ERR_NONE;
	// wtf is this?
	_pPADStatus->button = PAD_USE_ORIGIN;

	// try lock
	if (false == g_plugin.controls_crit.TryEnter())
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
		g_plugin.controller_interface.UpdateOutput();
		g_plugin.controller_interface.UpdateInput();
	}
	_last_numPAD = _numPAD;
	
	// get input
	((GCPad*)g_plugin.controllers[_numPAD])->GetInput(_pPADStatus);

	// leave
	g_plugin.controls_crit.Leave();
}

// __________________________________________________________________________________________________
// Function: PAD_Rumble
// Purpose:  Pad rumble!
// input:	 PAD number, Command type (Stop=0, Rumble=1, Stop Hard=2) and strength of Rumble
// output:   none
//
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	// enter
	if ( g_plugin.controls_crit.TryEnter() )
	{
		// TODO: this has potential to not stop rumble if user is messing with GUI at the perfect time
		// set rumble
		((GCPad*)g_plugin.controllers[ _numPAD ])->SetOutput( 1 == _uType && _uStrength > 2 );

		// leave
		g_plugin.controls_crit.Leave();
	}
}
