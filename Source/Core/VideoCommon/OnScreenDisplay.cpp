// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>
#include <mutex>
#include <string>

#include "imgui.h"

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/OnScreenDisplay.h"

namespace OSD
{
constexpr float LEFT_MARGIN = 10.0f;    // Pixels to the left of OSD messages.
constexpr float TOP_MARGIN = 10.0f;     // Pixels above the first OSD message.
constexpr float WINDOW_PADDING = 4.0f;  // Pixels between subsequent OSD messages.

struct Message
{
  Message() {}
  Message(const std::string& text_, u32 timestamp_, u32 color_)
      : text(text_), timestamp(timestamp_), color(color_)
  {
  }
  std::string text;
  u32 timestamp;
  u32 color;
};
static std::multimap<MessageType, Message> s_messages;
static std::mutex s_messages_mutex;

static ImVec4 RGBAToImVec4(const u32 rgba)
{
  return ImVec4(static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 8) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 0) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 24) & 0xFF) / 255.0f);
}

static float DrawMessage(int index, const Message& msg, const ImVec2& position, int time_left)
{
  // We have to provide a window name, and these shouldn't be duplicated.
  // So instead, we generate a name based on the number of messages drawn.
  const std::string window_name = StringFromFormat("osd_%d", index);

  // The size must be reset, otherwise the length of old messages could influence new ones.
  ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

  // Gradually fade old messages away.
  const float alpha = std::min(1.0f, std::max(0.0f, time_left / 1024.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

  float window_height = 0.0f;
  if (ImGui::Begin(window_name.c_str(), nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    // Use %s in case message contains %.
    ImGui::TextColored(RGBAToImVec4(msg.color), "%s", msg.text.c_str());
    window_height =
        ImGui::GetWindowSize().y + (WINDOW_PADDING * ImGui::GetIO().DisplayFramebufferScale.y);
  }

  ImGui::End();
  ImGui::PopStyleVar();

  return window_height;
}

void AddTypedMessage(MessageType type, const std::string& message, u32 ms, u32 rgba)
{
  std::lock_guard<std::mutex> lock(s_messages_mutex);
  s_messages.erase(type);
  s_messages.emplace(type, Message(message, Common::Timer::GetTimeMs() + ms, rgba));
}

void AddMessage(const std::string& message, u32 ms, u32 rgba)
{
  std::lock_guard<std::mutex> lock(s_messages_mutex);
  s_messages.emplace(MessageType::Typeless,
                     Message(message, Common::Timer::GetTimeMs() + ms, rgba));
}

void DrawMessages()
{
  if (!SConfig::GetInstance().bOnScreenDisplayMessages)
    return;

  {
    std::lock_guard<std::mutex> lock(s_messages_mutex);

    const u32 now = Common::Timer::GetTimeMs();
    float current_x = LEFT_MARGIN * ImGui::GetIO().DisplayFramebufferScale.x;
    float current_y = TOP_MARGIN * ImGui::GetIO().DisplayFramebufferScale.y;
    int index = 0;

    auto it = s_messages.begin();
    while (it != s_messages.end())
    {
      const Message& msg = it->second;
      const int time_left = static_cast<int>(msg.timestamp - now);
      current_y += DrawMessage(index++, msg, ImVec2(current_x, current_y), time_left);

      if (time_left <= 0)
        it = s_messages.erase(it);
      else
        ++it;
    }
  }
}

void ClearMessages()
{
  std::lock_guard<std::mutex> lock(s_messages_mutex);
  s_messages.clear();
}
}  // namespace OSD
