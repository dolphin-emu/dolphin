// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/Common.h"
#include "Core/ConfigManager.h"
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
	if (s_config.controllers.empty())
		for (unsigned int i = 0; i < 4; ++i)
			s_config.controllers.push_back(new GCKeyboard(i));

	g_controller_interface.Initialize(hwnd);

	// load the saved controller config
	s_config.LoadConfig(true);
}

void LoadConfig()
{
	s_config.LoadConfig(true);
}

void GetStatus(u8 _port, KeyboardStatus* _pKeyboardStatus)
{
	memset(_pKeyboardStatus, 0, sizeof(*_pKeyboardStatus));
	_pKeyboardStatus->err = PAD_ERR_NONE;

	// get input
	((GCKeyboard*)s_config.controllers[_port])->GetInput(_pKeyboardStatus);
}

}
