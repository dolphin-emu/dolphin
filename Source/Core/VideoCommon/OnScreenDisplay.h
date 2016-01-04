// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>

#include "Common/CommonTypes.h"

namespace OSD
{
// On-screen message display (colored yellow by default)
void AddMessage(const std::string& str, u32 ms = 2000, u32 rgba = 0xFFFFFF30);
void DrawMessages(); // draw the current messages on the screen. Only call once per frame.
void ClearMessages();

// On-screen callbacks
enum class CallbackType
{
	Initialization,
	OnFrame,
	Shutdown
};
using Callback = std::function<void()>;

void AddCallback(CallbackType type, Callback cb);
void DoCallbacks(CallbackType type);
}  // namespace OSD
