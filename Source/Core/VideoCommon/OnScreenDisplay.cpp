// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/OnScreenDisplay.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <string>

#include <fmt/format.h>
#include <imgui.h>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"

namespace OSD
{
constexpr float LEFT_MARGIN = 10.0f;    // Pixels to the left of OSD messages.
constexpr float TOP_MARGIN = 10.0f;     // Pixels above the first OSD message.
constexpr float WINDOW_PADDING = 4.0f;  // Pixels between subsequent OSD messages.

struct Message
{
  Message() = default;
  Message(std::string text_, u32 timestamp_, u32 color_)
      : text(std::move(text_)), timestamp(timestamp_), color(color_)
  {
  }
  std::string text;
  u32 timestamp = 0;
  u32 color = 0;
};
static std::multimap<MessageType, Message> s_messages;
static std::mutex s_messages_mutex;

static ImVec4 ARGBToImVec4(const u32 argb)
{
  return ImVec4(static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 0) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 24) & 0xFF) / 255.0f);
}

static float DrawMessage(int index, const Message& msg, const ImVec2& position, int time_left)
{
  // We have to provide a window name, and these shouldn't be duplicated.
  // So instead, we generate a name based on the number of messages drawn.
  const std::string window_name = fmt::format("osd_{}", index);

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
    ImGui::TextColored(ARGBToImVec4(msg.color), "%s", msg.text.c_str());
    window_height =
        ImGui::GetWindowSize().y + (WINDOW_PADDING * ImGui::GetIO().DisplayFramebufferScale.y);
  }

  ImGui::End();
  ImGui::PopStyleVar();

  return window_height;
}

void AddTypedMessage(MessageType type, std::string message, u32 ms, u32 argb)
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.erase(type);
  s_messages.emplace(type, Message(std::move(message), Common::Timer::GetTimeMs() + ms, argb));
}

void AddMessage(std::string message, u32 ms, u32 argb)
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.emplace(MessageType::Typeless,
                     Message(std::move(message), Common::Timer::GetTimeMs() + ms, argb));
}

void DrawMessages()
{
  const bool draw_messages = SConfig::GetInstance().bOnScreenDisplayMessages;
  const u32 now = Common::Timer::GetTimeMs();
  const float current_x = LEFT_MARGIN * ImGui::GetIO().DisplayFramebufferScale.x;
  float current_y = TOP_MARGIN * ImGui::GetIO().DisplayFramebufferScale.y;
  int index = 0;

  std::lock_guard lock{s_messages_mutex};

  for (auto it = s_messages.begin(); it != s_messages.end();)
  {
    const Message& msg = it->second;
    const int time_left = static_cast<int>(msg.timestamp - now);

    if (time_left <= 0)
    {
      it = s_messages.erase(it);
      continue;
    }
    else
    {
      ++it;
    }

    if (draw_messages)
      current_y += DrawMessage(index++, msg, ImVec2(current_x, current_y), time_left);
  }
}

void ClearMessages()
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.clear();
}
}  // namespace OSD
