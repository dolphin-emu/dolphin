// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/Movie.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace Wiimote
{

static InputConfig s_config(WIIMOTE_INI_NAME, _trans("Wiimote"), "Wiimote");
static int s_last_number = 4;

InputConfig* GetConfig()
{
	return &s_config;
}

void Shutdown()
{
	for (const ControllerEmu* i : s_config.controllers)
	{
		delete i;
	}
	s_config.controllers.clear();

	WiimoteReal::Stop();

	g_controller_interface.Shutdown();
}

// if plugin isn't initialized, init and load config
void Initialize(void* const hwnd, bool wait)
{
	// add 4 wiimotes
	for (unsigned int i = WIIMOTE_CHAN_0; i<MAX_BBMOTES; ++i)
		s_config.controllers.push_back(new WiimoteEmu::Wiimote(i));

	g_controller_interface.Initialize(hwnd);

	s_config.LoadConfig(false);

	WiimoteReal::Initialize(wait);

	// reload Wiimotes with our settings
	if (Movie::IsMovieActive())
		Movie::ChangeWiiPads();
}

void Resume()
{
	WiimoteReal::Resume();
}

void Pause()
{
	WiimoteReal::Pause();
}


// __________________________________________________________________________________________________
// Function: ControlChannel
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID CONTROL channel.
//
// Inputs:   _number    [Description needed]
//           _channelID [Description needed]
//           _pData     [Description needed]
//           _Size      [Description needed]
//
// Output:   none
//
void ControlChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[_number])
		((WiimoteEmu::Wiimote*)s_config.controllers[_number])->ControlChannel(_channelID, _pData, _Size);
}

// __________________________________________________________________________________________________
// Function: InterruptChannel
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID INTERRUPT channel.
//
// Inputs:   _number    [Description needed]
//           _channelID [Description needed]
//           _pData     [Description needed]
//           _Size      [Description needed]
//
// Output:   none
//
void InterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[_number])
		((WiimoteEmu::Wiimote*)s_config.controllers[_number])->InterruptChannel(_channelID, _pData, _Size);
}

// __________________________________________________________________________________________________
// Function: Update
// Purpose:  This function is called periodically by the Core. // TODO: Explain why.
// input:    _number: [Description needed]
// output:   none
//
void Update(int _number)
{
	//PanicAlert( "Wiimote_Update" );

	// TODO: change this to a try_to_lock, and make it give empty input on failure
	std::lock_guard<std::recursive_mutex> lk(s_config.controls_lock);

	if (_number <= s_last_number)
	{
		g_controller_interface.UpdateOutput();
		g_controller_interface.UpdateInput();
	}
	s_last_number = _number;

	if (WIIMOTE_SRC_EMU & g_wiimote_sources[_number])
		((WiimoteEmu::Wiimote*)s_config.controllers[_number])->Update();
	else
		WiimoteReal::Update(_number);
}

// __________________________________________________________________________________________________
// Function: GetAttached
// Purpose:  Get mask of attached pads (eg: controller 1 & 4 -> 0x9)
// input:    none
// output:   The number of attached pads
//
unsigned int GetAttached()
{
	unsigned int attached = 0;
	for (unsigned int i=0; i<MAX_BBMOTES; ++i)
		if (g_wiimote_sources[i])
			attached |= (1 << i);
	return attached;
}

// ___________________________________________________________________________
// Function: DoState
// Purpose:  Saves/load state
// input/output: ptr: [Description Needed]
// input: mode        [Description needed]
//
void DoState(u8 **ptr, PointerWrap::Mode mode)
{
	// TODO:

	PointerWrap p(ptr, mode);
	p.Do(s_last_number);
	for (unsigned int i=0; i<MAX_BBMOTES; ++i)
		((WiimoteEmu::Wiimote*)s_config.controllers[i])->DoState(p);
}

// ___________________________________________________________________________
// Function: EmuStateChange
// Purpose: Notifies the plugin of a change in emulation state
// input:    newState - The new state for the Wiimote to change to.
// output:   none
//
void EmuStateChange(EMUSTATE_CHANGE newState)
{
	// TODO
	WiimoteReal::StateChange(newState);
}

}
