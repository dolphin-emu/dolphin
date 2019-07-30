// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"

namespace OSD
{
static std::multimap<MessageType, Message> s_messages;
static std::mutex s_messages_mutex;

void AddTypedMessage(MessageType type, const std::string& message, u32 ms, u32 rgba)
{
  if (SConfig::GetInstance().bOnScreenDisplayMessages)
  {
    std::lock_guard<std::mutex> lock(s_messages_mutex);
    s_messages.erase(type);
    u32 timestamp = Common::Timer::GetTimeMs() + ms;
    s_messages.emplace(type, Message(message, timestamp, rgba));
  }
}

void AddMessage(const std::string& message, u32 ms, u32 rgba)
{
  if (SConfig::GetInstance().bOnScreenDisplayMessages)
  {
    std::lock_guard<std::mutex> lock(s_messages_mutex);
    u32 timestamp = Common::Timer::GetTimeMs() + ms;
    s_messages.emplace(MessageType::Typeless, Message(message, timestamp, rgba));
  }
}

void DrawMessage(const Message& msg, int top, int left, int time_left)
{
  float alpha = std::min(1.0f, std::max(0.0f, time_left / 1024.0f));
  u32 color = (msg.color & 0xFFFFFF) | ((u32)((msg.color >> 24) * alpha) << 24);
  g_renderer->RenderText(msg.text, left, top, color);
}

void DrawMessages()
{
  if (SConfig::GetInstance().bOnScreenDisplayMessages)
  {
    int left = 10, top = 32;
    u32 now = Common::Timer::GetTimeMs();
    std::lock_guard<std::mutex> lock(s_messages_mutex);
    auto it = s_messages.begin();
    while (it != s_messages.end())
    {
      const Message& msg = it->second;
      int time_left = (int)(msg.timestamp - now);
      DrawMessage(msg, top, left, time_left);

      if (time_left <= 0)
        it = s_messages.erase(it);
      else
        ++it;
      top += 15;
    }
  }
}

void ClearMessages()
{
  std::lock_guard<std::mutex> lock(s_messages_mutex);
  s_messages.clear();
}
}  // namespace OSD
