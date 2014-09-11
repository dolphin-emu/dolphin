// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
	Message(const std::string& s, u32 ts) : str(s), timestamp(ts) {}

	std::string str;
	u32 timestamp;
};

static std::multimap<CallbackType, Callback> s_callbacks;
static std::list<Message> s_msgList;

void AddMessage(const std::string& str, u32 ms)
{
	s_msgList.push_back(Message(str, Common::Timer::GetTimeMs() + ms));
}

void DrawMessages()
{
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bOnScreenDisplayMessages)
		return;

	int left = 25, top = 15;
	auto it = s_msgList.begin();
	while (it != s_msgList.end())
	{
		int time_left = (int)(it->timestamp - Common::Timer::GetTimeMs());
		u32 alpha = 255;

		if (time_left < 1024)
		{
			alpha = time_left >> 2;
			if (time_left < 0)
				alpha = 0;
		}

		alpha <<= 24;

		g_renderer->RenderText(it->str, left + 1, top + 1, 0x000000 | alpha);
		g_renderer->RenderText(it->str, left, top, 0xffff30 | alpha);
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
