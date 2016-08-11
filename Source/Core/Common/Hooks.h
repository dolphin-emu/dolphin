// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/HookedEvent.h"

// This macro will be used multiple times to generate everything we need for each hook
#define HOOKABLE_EVENT_TABLE(HOOK, ARG)                                                            \
  HOOK(ControllerHotplugEvent)                                                                     \
  HOOK(XFBCopyEvent, ARG(u32, xfbAddr), ARG(u32, xfbStride), ARG(u32, xfbHeight))

namespace Hook
{
// Generate the hook decelerations
#define TYPES_ONLY(TYPE, NAME) TYPE
#define DEFINE_EXTERNS(NAME, ...) extern HookableEvent<__VA_ARGS__> NAME;
HOOKABLE_EVENT_TABLE(DEFINE_EXTERNS, TYPES_ONLY)

#undef DEFINE_EXTERNS
#undef TYPES_ONLY
}