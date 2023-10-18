// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Subtitles/SubtitleEntry.h"

#include <algorithm>
#include <fmt/format.h>
#include <string>

#include "Common/CommonTypes.h"
#include "Subtitles/Helpers.h"

namespace Subtitles
{
// Ensure lines are sorted in reverse to simplify querying
void SubtitleEntryGroup::Preprocess()
{
  for (auto i = 0; i < subtitleLines.size(); i++)
  {
    hasOffsets |= subtitleLines[i].Offset > 0;
    hasTimestamps |= subtitleLines[i].Timestamp > 0;
  }

  if (hasOffsets)
  {
    std::sort(subtitleLines.begin(), subtitleLines.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.Offset > rhs.Offset; });
  }
  // Offsets override Timestamps
  else if (hasTimestamps)
  {
    std::sort(subtitleLines.begin(), subtitleLines.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.Timestamp > rhs.Timestamp; });
  }
}
SubtitleEntry* SubtitleEntryGroup::GetSubtitle(u32 offset)
{
  if (subtitleLines.empty())
    return nullptr;

  if (hasOffsets)
  {
    return GetSubtitleForRelativeOffset(offset);
  }
  if (hasTimestamps)
  {
    if (offset == 0)
    {
      // restart timer if file is being read from start
      timer.Start();
    }
    // TODO do sync emulated time with real time, or just ingore this issue?
    auto timestamp = timer.ElapsedMs();
    return GetSubtitleForRelativeTimestamp(timestamp);
  }

  return &subtitleLines[0];
}
SubtitleEntry* SubtitleEntryGroup::GetSubtitleForRelativeOffset(u32 offset)
{
  // from latest to earliest
  for (auto i = 0; i < subtitleLines.size(); i++)
  {
    // find first translation that covers current offset
    if (offset >= subtitleLines[i].Offset)
    {
      // if range is open, or offset is in range
      if (subtitleLines[i].OffsetEnd == 0 || subtitleLines[i].OffsetEnd >= offset)
      {
        return &subtitleLines[i];
      }
      else
      {
        return nullptr;
      }
    }
  }

  return nullptr;
}
SubtitleEntry* SubtitleEntryGroup::GetSubtitleForRelativeTimestamp(u64 timestamp)
{
  //if Subttile log is enabled, display timestamp for easier subtitle time aligning
  OSDInfo(fmt::format("Timestamp: {}", timestamp));

  // from latest to earliest
  for (auto i = 0; i < subtitleLines.size(); i++)
  {
    // find first translation that covers current offset
    if (timestamp >= subtitleLines[i].Timestamp)
    {
      // use display time as treshold
      auto endstamp = subtitleLines[i].Timestamp + subtitleLines[i].Miliseconds;
      if (endstamp >= timestamp)
      {
        return &subtitleLines[i];
      }
      else
      {
        return nullptr;
      }
    }
  }

  return nullptr;
}
void SubtitleEntryGroup::Add(SubtitleEntry& tl)
{
  subtitleLines.push_back(tl);
}

SubtitleEntry::SubtitleEntry()
    : Filename(""), Text(""), Miliseconds(0), Color(0), Enabled(false), AllowDuplicate(false),
      Scale(1), Offset(0), OffsetEnd(0), DisplayOnTop(false), Timestamp(0)
{
}
SubtitleEntry::SubtitleEntry(std::string& filename, std::string& text, u32 miliseconds, u32 color,
                             bool enabled, bool allowDuplicates, float scale, u32 offset,
                             u32 offsetEnd, bool displayOnTop, u64 timestamp)
    : Filename(filename), Text(text), Miliseconds(miliseconds), Color(color), Enabled(enabled),
      AllowDuplicate(allowDuplicates), Scale(scale), Offset(offset), OffsetEnd(offsetEnd),
      DisplayOnTop(displayOnTop), Timestamp(timestamp)
{
}
bool SubtitleEntry::IsOffset()
{
  return Offset > 0;
}
}  // namespace Subtitles
