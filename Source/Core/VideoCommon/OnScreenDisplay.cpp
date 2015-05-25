// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"


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

static std::multimap<CallbackType, Callback> s_callbacks;
static std::list<Message> s_msgList;

void AddMessage(const std::string& str, u32 ms, u32 rgba)
{
	s_msgList.emplace_back(str, Common::Timer::GetTimeMs() + ms, rgba);
}

void DrawMessages()
{
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bOnScreenDisplayMessages)
		return;

	int left = 25, top = 15;
	auto it = s_msgList.begin();
	while (it != s_msgList.end())
	{
		int time_left = (int)(it->m_timestamp - Common::Timer::GetTimeMs());
		float alpha = std::max(1.0f, std::min(0.0f, time_left / 1024.0f));
		u32 color = (it->m_rgba & 0xFFFFFF) | ((u32)((it->m_rgba >> 24) * alpha) << 24);

		g_renderer->RenderText(it->m_str, left, top, color);

		top += 15;

		if (time_left <= 0)
			it = s_msgList.erase(it);
		else
			++it;
	}
}

void ClearMessages()
{
	s_msgList.clear();
}

// On-Screen Display Callbacks
void AddCallback(CallbackType type, Callback cb)
{
	s_callbacks.insert(std::pair<CallbackType, Callback>(type, cb));
}

void DoCallbacks(CallbackType type)
{
	auto it_bounds = s_callbacks.equal_range(type);
	for (auto it = it_bounds.first; it != it_bounds.second; ++it)
	{
		it->second();
	}

	// Wipe all callbacks on shutdown
	if (type == OSD_SHUTDOWN)
		s_callbacks.clear();
}

}  // namespace
