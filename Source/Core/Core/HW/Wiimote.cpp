// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"

#include "Core/ConfigManager.h"
#include "Core/Movie.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace Wiimote
{

static InputPlugin g_plugin(WIIMOTE_INI_NAME, _trans("Wiimote"), "Wiimote");
InputPlugin *GetPlugin()
{
	return &g_plugin;
}

void Shutdown()
{
	for (const ControllerEmu* i : g_plugin.controllers)
	{
		delete i;
	}
	g_plugin.controllers.clear();

	WiimoteReal::Stop();

	g_controller_interface.Shutdown();
}

// if plugin isn't initialized, init and load config
void Initialize(void* const hwnd, bool wait)
{
	// add 4 wiimotes
	for (unsigned int i = WIIMOTE_CHAN_0; i<MAX_BBMOTES; ++i)
		g_plugin.controllers.push_back(new WiimoteEmu::Wiimote(i));


	g_controller_interface.SetHwnd(hwnd);
	g_controller_interface.Initialize();

	g_plugin.LoadConfig(false);

	WiimoteReal::Initialize(wait);

	// reload Wiimotes with our settings
	if (Movie::IsPlayingInput() || Movie::IsRecordingInput())
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
// Inputs:   number    [Description needed]
//           channelID [Description needed]
//           pData     [Description needed]
//           Size      [Description needed]
//
// Output:   none
//
void ControlChannel(int number, u16 channelID, const void* pData, u32 Size)
{
	if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[number])
		((WiimoteEmu::Wiimote*)g_plugin.controllers[number])->ControlChannel(channelID, pData, Size);
}

// __________________________________________________________________________________________________
// Function: InterruptChannel
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID INTERRUPT channel.
//
// Inputs:   number    [Description needed]
//           channelID [Description needed]
//           pData     [Description needed]
//           Size      [Description needed]
//
// Output:   none
//
void InterruptChannel(int number, u16 channelID, const void* pData, u32 Size)
{
	if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[number])
		((WiimoteEmu::Wiimote*)g_plugin.controllers[number])->InterruptChannel(channelID, pData, Size);
}

// __________________________________________________________________________________________________
// Function: Update
// Purpose:  This function is called periodically by the Core. // TODO: Explain why.
// input:    number: [Description needed]
// output:   none
//
void Update(int number)
{
	//PanicAlert( "Wiimote_Update" );

	// TODO: change this to a try_to_lock, and make it give empty input on failure
	std::lock_guard<std::recursive_mutex> lk(g_plugin.controls_lock);

	static int last_number = 4;
	if (number <= last_number)
	{
		g_controller_interface.UpdateOutput();
		g_controller_interface.UpdateInput();
	}
	last_number = number;

	if (WIIMOTE_SRC_EMU & g_wiimote_sources[number])
		((WiimoteEmu::Wiimote*)g_plugin.controllers[number])->Update();
	else
		WiimoteReal::Update(number);
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
	for (unsigned int i=0; i<MAX_BBMOTES; ++i)
		((WiimoteEmu::Wiimote*)g_plugin.controllers[i])->DoState(p);
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
