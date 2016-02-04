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
	Message(const std::string& s, u32 ts, u32 rgba) : m_str(s), m_timestamp(ts), m_rgba(rgba) {}

	std::string m_str;
	u32 m_timestamp;
	u32 m_rgba;
};

enum class MessageType {
	NetPlayPing,

	// This entry must be kept last so that persistent typed messages are displayed before other messages
	Typeless,
};

namespace Color
{
constexpr u32 CYAN   = 0xFF00FFFF;
constexpr u32 GREEN  = 0xFF00FF00;
constexpr u32 RED    = 0xFFFF0000;
constexpr u32 YELLOW = 0xFFFFFF30;
};

namespace Duration
{
constexpr u32 SHORT     = 2000;
constexpr u32 NORMAL    = 5000;
constexpr u32 VERY_LONG = 10000;
};

// On-screen message display (colored yellow by default)
void AddMessage(const std::string& message, u32 ms = Duration::SHORT, u32 rgba = Color::YELLOW);
void AddTypedMessage(MessageType type, const std::string &message, u32 ms = Duration::SHORT,
					 u32 rgba = Color::YELLOW);
void DrawMessage(const Message& msg, int top, int left, int time_left); // draw one message
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
