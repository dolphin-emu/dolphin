// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

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
	for (unsigned int i=0; i<4; ++i)
		s_config.controllers.push_back(new GCPad(i));

	g_controller_interface.Initialize(hwnd);

	// load the saved controller config
	s_config.LoadConfig(true);
}

void GetStatus(u8 _numPAD, GCPadStatus* _pPADStatus)
{
	memset(_pPADStatus, 0, sizeof(*_pPADStatus));
	_pPADStatus->err = PAD_ERR_NONE;

	std::unique_lock<std::recursive_mutex> lk(s_config.controls_lock, std::try_to_lock);

	if (!lk.owns_lock())
	{
		// if gui has lock (messing with controls), skip this input cycle
		// center axes and return
		_pPADStatus->stickX = GCPadStatus::MAIN_STICK_CENTER_X;
		_pPADStatus->stickY = GCPadStatus::MAIN_STICK_CENTER_Y;
		_pPADStatus->substickX = GCPadStatus::C_STICK_CENTER_X;
		_pPADStatus->substickY = GCPadStatus::C_STICK_CENTER_Y;
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
	((GCPad*)s_config.controllers[_numPAD])->GetInput(_pPADStatus);
}

// __________________________________________________________________________________________________
// Function: Rumble
// Purpose:  Pad rumble!
// input:    _numPad    - Which pad to rumble.
//           _uType     - Command type (Stop=0, Rumble=1, Stop Hard=2).
//           _uStrength - Strength of the Rumble
// output:   none
//
void Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	std::unique_lock<std::recursive_mutex> lk(s_config.controls_lock, std::try_to_lock);

	if (lk.owns_lock())
	{
		// TODO: this has potential to not stop rumble if user is messing with GUI at the perfect time
		// set rumble
		if (1 == _uType && _uStrength > 2)
		{
			((GCPad*)s_config.controllers[ _numPAD ])->SetOutput(255);
		}
		else
		{
			((GCPad*)s_config.controllers[ _numPAD ])->SetOutput(0);
		}
	}
}

// __________________________________________________________________________________________________
// Function: Motor
// Purpose:  For devices with constant Force feedback
// input:    _numPAD    - The pad to operate on
//           _uType     - 06 = Motor On, 04 = Motor Off
//           _uStrength - 00 = Left Strong, 127 = Left Weak, 128 = Right Weak, 255 = Right Strong
// output:   none
//
void Motor(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	std::unique_lock<std::recursive_mutex> lk(s_config.controls_lock, std::try_to_lock);

	if (lk.owns_lock())
	{
		// TODO: this has potential to not stop rumble if user is messing with GUI at the perfect time
		// set rumble
		if (_uType == 6)
		{
			((GCPad*)s_config.controllers[ _numPAD ])->SetMotor(_uStrength);
		}
	}
}

bool GetMicButton(u8 pad)
{

	std::unique_lock<std::recursive_mutex> lk(s_config.controls_lock, std::try_to_lock);

	if (!lk.owns_lock())
		return false;

	return ((GCPad*)s_config.controllers[pad])->GetMicButton();
}

}
