// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"


namespace OSD
{

static std::multimap<CallbackType, Callback> s_callbacks;
static std::map<int, Message> s_msg_list;
static int s_typeless_message_index;

void AddTypedMessage(int type, const std::string &message, u32 ms, u32 rgba)
{
	s_msg_list[type] = Message(message, Common::Timer::GetTimeMs() + ms, rgba);
}

void AddMessage(const std::string& message, u32 ms, u32 rgba)
{
	s_typeless_message_index = (s_typeless_message_index % (1 << 31)) - 1;
	AddTypedMessage(s_typeless_message_index, message, ms, rgba);
}

void DrawMessage(Message* msg, int top, int left, int time_left)
{
	float alpha = std::min(1.0f, std::max(0.0f, time_left / 1024.0f));
	u32 color = (msg->m_rgba & 0xFFFFFF) | ((u32)((msg->m_rgba >> 24) * alpha) << 24);

	g_renderer->RenderText(msg->m_str, left, top, color);
}

void DrawMessages()
{
	if (!SConfig::GetInstance().bOnScreenDisplayMessages)
		return;

	int left = 20, top = 35;
	u32 now = Common::Timer::GetTimeMs();

	auto namedIt = s_msg_list.rbegin();
	while (namedIt != s_msg_list.rend())
	{
		Message msg = namedIt->second;
		int time_left = (int)(msg.m_timestamp - now);
		DrawMessage(&msg, top, left, time_left);

		if (time_left <= 0)
			s_msg_list.erase(namedIt->first);

		++namedIt;

		top += 15;
	}
}

void ClearMessages()
{
	s_msg_list.clear();
}

// On-Screen Display Callbacks
void AddCallback(CallbackType type, Callback cb)
{
	s_callbacks.emplace(type, cb);
}

void DoCallbacks(CallbackType type)
{
	auto it_bounds = s_callbacks.equal_range(type);
	for (auto it = it_bounds.first; it != it_bounds.second; ++it)
	{
		it->second();
	}

	// Wipe all callbacks on shutdown
	if (type == CallbackType::Shutdown)
		s_callbacks.clear();
}

}  // namespace
