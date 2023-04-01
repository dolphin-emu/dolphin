// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/Movie.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace Wiimote
{

static InputConfig s_config(WIIMOTE_INI_NAME, _trans("Wiimote"), "Wiimote");

InputConfig* GetConfig()
{
	return &s_config;
}

void Shutdown()
{
	s_config.ClearControllers();

	WiimoteReal::Stop();

	g_controller_interface.Shutdown();
}

void Initialize(void* const hwnd, bool wait)
{
	if (s_config.ControllersNeedToBeCreated())
	{
		for (unsigned int i = WIIMOTE_CHAN_0; i < MAX_BBMOTES; ++i)
			s_config.CreateController<WiimoteEmu::Wiimote>(i);
	}

	g_controller_interface.Initialize(hwnd);

	s_config.LoadConfig(false);

	WiimoteReal::Initialize(wait);

	// Reload Wiimotes with our settings
	if (Movie::IsMovieActive())
		Movie::ChangeWiiPads();
}

void ResetAllWiimotes()
{
	for (int i = WIIMOTE_CHAN_0; i < MAX_BBMOTES; ++i)
	        static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->Reset();
}

void LoadConfig()
{
	s_config.LoadConfig(false);
}


void Resume()
{
	WiimoteReal::Resume();
}

void Pause()
{
	WiimoteReal::Pause();
}


// An L2CAP packet is passed from the Core to the Wiimote on the HID CONTROL channel.
void ControlChannel(int number, u16 channel_id, const void* data, u32 size)
{
	if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[number])
		static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->ControlChannel(channel_id, data, size);
}

// An L2CAP packet is passed from the Core to the Wiimote on the HID INTERRUPT channel.
void InterruptChannel(int number, u16 channel_id, const void* data, u32 size)
{
	if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[number])
		static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->InterruptChannel(channel_id, data, size);
}

// This function is called periodically by the Core to update Wiimote state.
void Update(int number, bool connected)
{
	if (connected)
	{
		if (WIIMOTE_SRC_EMU & g_wiimote_sources[number])
			static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->Update();
		else
			WiimoteReal::Update(number);
	}
	else
	{
		if (WIIMOTE_SRC_EMU & g_wiimote_sources[number])
			static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->ConnectOnInput();

		if (WIIMOTE_SRC_REAL & g_wiimote_sources[number])
			WiimoteReal::ConnectOnInput(number);
	}
}

// Get a mask of attached the pads (eg: controller 1 & 4 -> 0x9)
unsigned int GetAttached()
{
	unsigned int attached = 0;
	for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
		if (g_wiimote_sources[i])
			attached |= (1 << i);
	return attached;
}

// Save/Load state
void DoState(PointerWrap& p)
{
	for (int i = 0; i < MAX_BBMOTES; ++i)
		static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->DoState(p);
}

// Notifies the plugin of a change in emulation state
void EmuStateChange(EMUSTATE_CHANGE newState)
{
	WiimoteReal::StateChange(newState);
}

}
