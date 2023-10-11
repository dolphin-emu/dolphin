// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

namespace Subtitles
{

struct SubtitleEntry
{
  std::string Filename;
  std::string Text;
  u32 Miliseconds;
  u32 Color;
  bool Enabled;
  bool AllowDuplicate;
  float Scale;
  u32 Offset;
  u32 OffsetEnd;
  bool DisplayOnTop;
  u64 Timestamp;

public:
  SubtitleEntry();
  SubtitleEntry(std::string& filename, std::string& text, u32 miliseconds, u32 color, bool enabled,
                bool allowDuplicates, float scale, u32 offset, u32 offsetEnd, bool displayOnTop,
                u64 timestamp);
  bool IsOffset();
};

/// <summary>
/// Helper struct to deal with multiple subtitle lines per file
/// </summary>
struct SubtitleEntryGroup
{
  Common::Timer timer;

  std::vector<SubtitleEntry> subtitleLines;
  bool hasOffsets = false;
  bool hasTimestamps = false;

  // Ensure lines are sorted in reverse to simplify querying
  void Preprocess();
  void Add(SubtitleEntry& tl);
  SubtitleEntry* GetSubtitle(u32 offset);

private:
  SubtitleEntry* GetSubtitleForRelativeOffset(u32 offset);
  SubtitleEntry* GetSubtitleForRelativeTimestamp(u64 timestamp);
};
}  // namespace Subtitles
