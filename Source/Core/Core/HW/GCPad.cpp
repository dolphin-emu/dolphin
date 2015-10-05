// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Common.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace Pad
{

static InputConfig s_config("GCPadNew", _trans("Pad"), "GCPad");
InputConfig* GetConfig()
{
	return &s_config;
}

void Shutdown()
{
	std::vector<ControllerEmu*>::const_iterator
		i = s_config.controllers.begin(),
		e = s_config.controllers.end();
	for ( ; i!=e; ++i )
		delete *i;
	s_config.controllers.clear();

	g_controller_interface.Shutdown();
}

// if plugin isn't initialized, init and load config
void Initialize(void* const hwnd)
{
	// add 4 gcpads
	if (s_config.controllers.empty())
		for (unsigned int i = 0; i < 4; ++i)
			s_config.controllers.push_back(new GCPad(i));

	g_controller_interface.Initialize(hwnd);

	// load the saved controller config
	s_config.LoadConfig(true);
}

void LoadConfig()
{
	s_config.LoadConfig(true);
}


void GetStatus(u8 _numPAD, GCPadStatus* _pPADStatus)
{
	memset(_pPADStatus, 0, sizeof(*_pPADStatus));
	_pPADStatus->err = PAD_ERR_NONE;

	// if we are on the next input cycle, update output and input
	static int _last_numPAD = 4;
	if (_numPAD <= _last_numPAD)
	{
		g_controller_interface.UpdateInput();
	}
	_last_numPAD = _numPAD;

	// get input
	((GCPad*)s_config.controllers[_numPAD])->GetInput(_pPADStatus);
}

void Rumble(u8 _numPAD, const ControlState strength)
{
	((GCPad*)s_config.controllers[ _numPAD ])->SetOutput(strength);
}

bool GetMicButton(u8 pad)
{
	return ((GCPad*)s_config.controllers[pad])->GetMicButton();
}

}
