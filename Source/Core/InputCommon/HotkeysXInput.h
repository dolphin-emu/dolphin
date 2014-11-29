// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "wx/string.h"

#include "Common/CommonTypes.h"

namespace HotkeysXInput
{

	void Update();
	void OnXInputPoll(u32* XInput_State); // XInput
	bool IsVRSettingsXInput(u32* XInput_State, int Id);
	bool IsHotkeyXInput(u32* XInput_State, int Id);
	u32 GetBinaryfromXInputIniStr(wxString ini_setting);
	wxString GetwxStringfromXInputIni(u32 ini_setting);
	bool IsXInputButtonSet(wxString button_string, int id);
	bool IsHotkeyXInputButtonSet(wxString button_string, int id);

}
