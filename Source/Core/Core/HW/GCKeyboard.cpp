// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCKeyboardEmu.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/KeyboardStatus.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace Keyboard
{

static InputConfig s_config("GCKeyNew", _trans("Keyboard"), "GCKey");
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
			s_config.CreateController<GCKeyboard>(i);
	}

	g_controller_interface.Initialize(hwnd);

	// Load the saved controller config
	s_config.LoadConfig(true);
}

void LoadConfig()
{
	s_config.LoadConfig(true);
}

void GetStatus(u8 port, KeyboardStatus* keyboard_status)
{
	memset(keyboard_status, 0, sizeof(*keyboard_status));
	keyboard_status->err = PAD_ERR_NONE;

	// Get input
	static_cast<GCKeyboard*>(s_config.GetController(port))->GetInput(keyboard_status);
}

}
