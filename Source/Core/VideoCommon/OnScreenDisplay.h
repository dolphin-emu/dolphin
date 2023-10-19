// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

#include "VideoCommon/AbstractTexture.h"

namespace OSD
{
enum class MessageStackDirection
{
  Downward = 1,
  Upward = 2,
  Rightward = 4,
  Leftward = 8,
};

enum class MessageType
{
  NetPlayPing,
  NetPlayBuffer,

  // This entry must be kept last so that persistent typed messages are
  // displayed before other messages
  Typeless,
};

struct Icon
{
  std::vector<u8> rgba_data;
  u32 width = 0;
  u32 height = 0;
};  // struct Icon

struct Message
{
  Message() = default;
  Message(std::string text_, u32 duration_, u32 color_, std::unique_ptr<Icon> icon_ = nullptr, float scale_ = 1)
      : text(std::move(text_)), duration(duration_), color(color_), icon(std::move(icon_)), scale(scale_)
  {
    timer.Start();
  }
  s64 TimeRemaining() const { return duration - timer.ElapsedMs(); }
  std::string text;
  Common::Timer timer;
  u32 duration = 0;
  bool ever_drawn = false;
  bool should_discard = false;
  u32 color = 0;
  std::unique_ptr<Icon> icon;
  std::unique_ptr<AbstractTexture> texture;
  float scale = 1;
};

struct OSDMessageStack
{
  OSDMessageStack(OSDMessageStack& t) {}

  ImVec2 initialPosOffset;
  MessageStackDirection dir;
  bool centered;
  bool reversed;
  std::string name;
  std::multimap<OSD::MessageType, OSD::Message> messages;

  OSDMessageStack() : OSDMessageStack(0, 0, MessageStackDirection::Downward, false, false, "") {}
  OSDMessageStack(float x_offset, float y_offset, MessageStackDirection dir, bool centered,
                  bool reversed, std::string name)
      : dir(dir), centered(centered), reversed(reversed), name(name)
  {
    initialPosOffset = ImVec2(x_offset, y_offset);
  }

  bool IsVertical()
  {
    return dir == MessageStackDirection::Downward || dir == MessageStackDirection::Upward;
  }

  bool HasMessage(std::string message, MessageType type = OSD::MessageType::Typeless)
  {
    for (auto it = messages.begin(); it != messages.end(); ++it)
    {
      if (type == it->first && message == it->second.text)
      {
        return true;
      }
    }
    return false;
  }
};

namespace Color
{
constexpr u32 CYAN = 0xFF00FFFF;
constexpr u32 GREEN = 0xFF00FF00;
constexpr u32 RED = 0xFFFF0000;
constexpr u32 YELLOW = 0xFFFFFF30;
};  // namespace Color

namespace Duration
{
constexpr u32 SHORT = 2000;
constexpr u32 NORMAL = 5000;
constexpr u32 VERY_LONG = 10000;
};  // namespace Duration

void AddMessageStack(OSDMessageStack& info);

// On-screen message display (colored yellow by default)
void AddMessage(std::string message, u32 ms = Duration::SHORT, u32 argb = Color::YELLOW,
                std::unique_ptr<Icon> icon = nullptr, std::string message_stack = "",
                bool prevent_duplicate = false, float scale = 1);
void AddTypedMessage(MessageType type, std::string message, u32 ms = Duration::SHORT,
                     u32 argb = Color::YELLOW, std::unique_ptr<Icon> icon = nullptr,
                     std::string message_stack = "", bool prevent_duplicate = false,
                     float scale = 1);

// Draw the current messages on the screen. Only call once per frame.
void DrawMessages();
void ClearMessages();

void SetObscuredPixelsLeft(int width);
void SetObscuredPixelsTop(int height);
}  // namespace OSD
