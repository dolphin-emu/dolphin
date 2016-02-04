// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>

#include "Common/CommonTypes.h"

namespace OSD
{

struct Message
{
	Message() {}
	Message(const std::string& s, u32 ts, u32 rgba) : m_str(s), m_timestamp(ts), m_rgba(rgba)
	{
	}

	std::string m_str;
	u32 m_timestamp;
	u32 m_rgba;
};

enum MessageName
{
	netplay_ping
};

enum MessageColor
{
	yellow = 0xFFFFFF30,
	cyan   = 0xFF00FFFF,
	red    = 0xFFFF0000,
	green  = 0xFF00FF00
};

enum MessageDuration
{
	mshort     = 2000,
	mlong      = 5000,
	mvery_long = 10000
};

// On-screen message display (colored yellow by default)
void AddMessage(const std::string& message, u32 ms = MessageDuration::mshort, u32 rgba = MessageColor::yellow);
void AddNamedMessage(MessageName name, const std::string& message, u32 ms = MessageDuration::mshort, u32 rgba = MessageColor::yellow);
void DrawMessage(Message* msg, int top, int left, int time_left); // draw one message
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
