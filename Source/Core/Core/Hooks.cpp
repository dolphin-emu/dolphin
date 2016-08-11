// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Hooks.h"

namespace Hook
{
HookableEvent<> ControllerHotplugEvent;
HookableEvent<u32, u32, u32> XFBCopyEvent;
}