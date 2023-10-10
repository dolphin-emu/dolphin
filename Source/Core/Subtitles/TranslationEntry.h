#pragma once
#include <string>
#include <Common/CommonTypes.h>
namespace Subtitles
{
struct TranslationEntry
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

public:
  TranslationEntry()
      : Filename(""), Text(""), Miliseconds(0), Color(0), Enabled(false), AllowDuplicate(false),
        Scale(1), Offset(0), OffsetEnd(0), DisplayOnTop(false)
  {
  }
  TranslationEntry(std::string filename, std::string text, u32 miliseconds, u32 color, bool enabled,
                   bool allowDuplicates, float scale, u32 offset, u32 offsetEnd, bool displayOnTop)
      : Filename(filename), Text(text), Miliseconds(miliseconds), Color(color), Enabled(enabled),
        AllowDuplicate(allowDuplicates), Scale(scale), Offset(offset), OffsetEnd(offsetEnd),
        DisplayOnTop(displayOnTop)
  {
  }
  bool IsOffset() { return Offset > 0; }
};

struct TranslationEntryGroup
{
  std::vector<TranslationEntry> translationLines;

  // Ensure lines are sorted to simplify querying
  void Sort()
  {
    std::sort(translationLines.begin(), translationLines.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.Offset > rhs.Offset; });
  }
  TranslationEntry* GetTLForRelativeOffset(u32 offset)
  {
    if (translationLines.empty())
    {
      return 0;
    }

    //if there is entry with offset=0 or offset=null
    if (!translationLines[translationLines.size() - 1].IsOffset())
    {
      //use it by default
      return &translationLines[translationLines.size() - 1];
    }

    //from latest to earliest
    for (auto i = 0; i < translationLines.size(); i++)
    {
      //find first translation that covers current offset
      if (offset >= translationLines[i].Offset)
      {
        //if range is open, or offset is in range
        if (translationLines[i].OffsetEnd == 0 || translationLines[i].OffsetEnd >= offset)
        {
          return &translationLines[i];
        }
        else
        {
          return 0;
        }
      }
    }

    return 0;
  }
  void Add(TranslationEntry tl) { translationLines.push_back(tl); }
};
}  // namespace Subtitles
