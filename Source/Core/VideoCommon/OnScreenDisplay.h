// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>

#include "Common/CommonTypes.h"

namespace OSD
{
// On-screen message display
void AddMessage(const std::string& str, u32 ms = 2000);
void DrawMessages(); // draw the current messages on the screen. Only call once per frame.
void ClearMessages();

// On-screen callbacks
enum CallbackType
{
	OSD_INIT = 0,
	OSD_ONFRAME,
	OSD_SHUTDOWN
};
typedef std::function<void()> Callback;

void AddCallback(CallbackType type, Callback cb);
void DoCallbacks(CallbackType type);
}  // namespace OSD
