// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Subtitles/SubtitleEntry.h"

#include <algorithm>
#include <string>

#include "Common/CommonTypes.h"

namespace Subtitles
{
// Ensure lines are sorted in reverse to simplify querying
void SubtitleEntryGroup::Sort()
{
  std::sort(subtitleLines.begin(), subtitleLines.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.Offset > rhs.Offset; });
}
SubtitleEntry* SubtitleEntryGroup::GetTLForRelativeOffset(u32 offset)
{
  if (subtitleLines.empty())
    return 0;

  // entry with offset=0 or offset=null is used by default
  if (!subtitleLines[subtitleLines.size() - 1].IsOffset())
    return &subtitleLines[subtitleLines.size() - 1];

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
        return 0;
      }
    }
  }

  return 0;
}
void SubtitleEntryGroup::Add(SubtitleEntry tl)
{
  subtitleLines.push_back(tl);
}

SubtitleEntry::SubtitleEntry()
    : Filename(""), Text(""), Miliseconds(0), Color(0), Enabled(false), AllowDuplicate(false),
      Scale(1), Offset(0), OffsetEnd(0), DisplayOnTop(false)
{
}
SubtitleEntry::SubtitleEntry(std::string filename, std::string text, u32 miliseconds,
                                    u32 color, bool enabled, bool allowDuplicates, float scale,
                                    u32 offset, u32 offsetEnd, bool displayOnTop)
    : Filename(filename), Text(text), Miliseconds(miliseconds), Color(color), Enabled(enabled),
      AllowDuplicate(allowDuplicates), Scale(scale), Offset(offset), OffsetEnd(offsetEnd),
      DisplayOnTop(displayOnTop)
{
}
bool SubtitleEntry::IsOffset()
{
  return Offset > 0;
}
}  // namespace Subtitles
