// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>

#include <imgui.h>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

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

struct Message
{
  Message() = default;
  Message(std::string text_, u32 duration_, u32 color_, float scale)
      : text(std::move(text_)), duration(duration_), color(color_), scale(scale)
  {
    timer.Start();
  }
  s64 TimeRemaining() const { return duration - timer.ElapsedMs(); }
  std::string text;
  Common::Timer timer;
  u32 duration = 0;
  bool ever_drawn = false;
  u32 color = 0;
  float scale = 1;
};

class OSDMessageStack
{
public:
  ImVec2 initialPosOffset;
  MessageStackDirection dir;
  bool centered;
  bool reversed;
  std::string name;
  std::multimap<OSD::MessageType, OSD::Message> messages;

  OSDMessageStack() : OSDMessageStack(0, 0, MessageStackDirection::Downward, false, false, "") {}
  OSDMessageStack(float _x_offset, float _y_offset, MessageStackDirection _dir, bool _centered,
                  bool _reversed, std::string _name)
  {
    initialPosOffset = ImVec2(_x_offset, _y_offset);
    dir = _dir;
    centered = _centered;
    reversed = _reversed;
    name = _name;
  }

  bool isVertical()
  {
    return dir == MessageStackDirection::Downward || dir == MessageStackDirection::Upward;
  }

  bool hasMessage(std::string message, MessageType type = OSD::MessageType::Typeless)
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

void AddMessageStack(OSDMessageStack info);

// On-screen message display (colored yellow by default)
void AddMessage(std::string message, u32 ms = Duration::SHORT, u32 argb = Color::YELLOW,
                std::string messageStack = "", bool preventDuplicate = false, float scale = 1);
void AddTypedMessage(MessageType type, std::string message, u32 ms = Duration::SHORT,
                     u32 argb = Color::YELLOW, std::string messageStack = "",
                     bool preventDuplicate = false, float scale = 1);

// Draw the current messages on the screen. Only call once per frame.
void DrawMessages();
void ClearMessages();

void SetObscuredPixelsLeft(int width);
void SetObscuredPixelsTop(int height);
}  // namespace OSD
