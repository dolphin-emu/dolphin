// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace HotkeysXInput
{

	void Update();
	void OnXInputPoll(u32* XInput_State); // XInput
	bool IsVRSettingsXInput(u32* XInput_State, int Id);

}
