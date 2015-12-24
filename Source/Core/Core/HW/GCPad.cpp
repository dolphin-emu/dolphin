// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
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
	s_config.ClearControllers();

	g_controller_interface.Shutdown();
}

void Initialize(void* const hwnd)
{
	if (s_config.ControllersNeedToBeCreated())
	{
		for (unsigned int i = 0; i < 4; ++i)
			s_config.CreateController<GCPad>(i);
	}

	g_controller_interface.Initialize(hwnd);

	// Load the saved controller config
	s_config.LoadConfig(INPUT_TYPE_PAD);
}

void LoadConfig()
{
	s_config.LoadConfig(INPUT_TYPE_PAD);
}


void GetStatus(u8 pad_num, GCPadStatus* pad_status)
{
	memset(pad_status, 0, sizeof(*pad_status));
	pad_status->err = PAD_ERR_NONE;

	// If we are on the next input cycle, update output and input
	static int last_pad_num = 4;
	if (pad_num <= last_pad_num)
	{
		g_controller_interface.UpdateInput();
	}
	last_pad_num = pad_num;

	// Get input
	static_cast<GCPad*>(s_config.GetController(pad_num))->GetInput(pad_status);
}

void Rumble(const u8 pad_num, const ControlState strength)
{
	static_cast<GCPad*>(s_config.GetController(pad_num))->SetOutput(strength);
}

bool GetMicButton(const u8 pad_num)
{
	return static_cast<GCPad*>(s_config.GetController(pad_num))->GetMicButton();
}

}
