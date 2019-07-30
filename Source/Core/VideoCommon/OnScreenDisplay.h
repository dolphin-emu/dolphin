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
    Message(const std::string& text_, u32 timestamp_, u32 color_)
      : text(text_), timestamp(timestamp_), color(color_)
    {
    }
    std::string text;
    u32 timestamp;
    u32 color;
};

enum class MessageType
{
  NetPlayPing,
  NetPlayBuffer,
  EFBScale,
  LogicOpsNotice,
  StrideNotice,
  OutOfRangeNotice,
  BoundingBoxNotice,
  GPUDecodeNotice,

  // This entry must be kept last so that persistent typed messages are
  // displayed before other messages
  Typeless,
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

// On-screen message display (colored yellow by default)
void AddMessage(const std::string& message, u32 ms = Duration::SHORT, u32 rgba = Color::YELLOW);
void AddTypedMessage(MessageType type, const std::string& message, u32 ms = Duration::SHORT, u32 rgba = Color::YELLOW);
void DrawMessage(const Message& msg, int top, int left, int time_left);  // draw one message
void DrawMessages();  // draw the current messages on the screen. Only call once
                      // per frame.
void ClearMessages();
}  // namespace OSD
