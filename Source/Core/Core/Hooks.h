// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/HookedEvent.h"

namespace Hook
{
extern HookableEvent<> ControllerHotplugEvent;
extern HookableEvent<u32, u32, u32> XFBCopyEvent;
}