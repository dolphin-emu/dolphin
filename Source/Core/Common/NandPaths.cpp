// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/NandPaths.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

namespace Common
{
std::string RootUserPath(FromWhichRoot from)
{
  int idx = from == FROM_CONFIGURED_ROOT ? D_WIIROOT_IDX : D_SESSION_WIIROOT_IDX;
  std::string dir = File::GetUserPath(idx);
  dir.pop_back();  // remove trailing path separator
  return dir;
}

static std::string RootUserPath(std::optional<FromWhichRoot> from)
{
  return from ? RootUserPath(*from) : "";
}

std::string GetImportTitlePath(u64 title_id, std::optional<FromWhichRoot> from)
{
  return RootUserPath(from) + fmt::format("/import/{:08x}/{:08x}", static_cast<u32>(title_id >> 32),
                                          static_cast<u32>(title_id));
}

std::string GetTicketFileName(u64 title_id, std::optional<FromWhichRoot> from)
{
  return fmt::format("{}/ticket/{:08x}/{:08x}.tik", RootUserPath(from),
                     static_cast<u32>(title_id >> 32), static_cast<u32>(title_id));
}

std::string GetV1TicketFileName(u64 title_id, std::optional<FromWhichRoot> from)
{
  return fmt::format("{}/ticket/{:08x}/{:08x}.tv1", RootUserPath(from),
                     static_cast<u32>(title_id >> 32), static_cast<u32>(title_id));
}

std::string GetTitlePath(u64 title_id, std::optional<FromWhichRoot> from)
{
  return fmt::format("{}/title/{:08x}/{:08x}", RootUserPath(from), static_cast<u32>(title_id >> 32),
                     static_cast<u32>(title_id));
}

std::string GetTitleDataPath(u64 title_id, std::optional<FromWhichRoot> from)
{
  return GetTitlePath(title_id, from) + "/data";
}

std::string GetTitleContentPath(u64 title_id, std::optional<FromWhichRoot> from)
{
  return GetTitlePath(title_id, from) + "/content";
}

std::string GetTMDFileName(u64 title_id, std::optional<FromWhichRoot> from)
{
  return GetTitleContentPath(title_id, from) + "/title.tmd";
}

std::string GetMiiDatabasePath(std::optional<FromWhichRoot> from)
{
  return fmt::format("{}/shared2/menu/FaceLib/RFL_DB.dat", RootUserPath(from));
}

bool IsTitlePath(const std::string& path, std::optional<FromWhichRoot> from, u64* title_id)
{
  std::string expected_prefix = RootUserPath(from) + "/title/";
  if (!path.starts_with(expected_prefix))
  {
    return false;
  }

  // Try to find a title ID in the remaining path.
  std::string subdirectory = path.substr(expected_prefix.size());
  std::vector<std::string> components = SplitString(subdirectory, '/');
  if (components.size() < 2)
  {
    return false;
  }

  u32 title_id_high, title_id_low;
  if (!AsciiToHex(components[0], title_id_high) || !AsciiToHex(components[1], title_id_low))
  {
    return false;
  }

  if (title_id != nullptr)
  {
    *title_id = (static_cast<u64>(title_id_high) << 32) | title_id_low;
  }
  return true;
}

static bool IsIllegalCharacter(char c)
{
  static const std::unordered_set<char> illegal_chars = {'\"', '*', '/',  ':', '<',
                                                         '>',  '?', '\\', '|', '\x7f'};
  return (c >= 0 && c <= 0x1F) || illegal_chars.find(c) != illegal_chars.end();
}

std::string EscapeFileName(const std::string& filename)
{
  // Prevent paths from containing special names like ., .., ..., ...., and so on
  if (std::all_of(filename.begin(), filename.end(), [](char c) { return c == '.'; }))
    return ReplaceAll(filename, ".", "__2e__");

  // Escape all double underscores since we will use double underscores for our escape sequences
  std::string filename_with_escaped_double_underscores = ReplaceAll(filename, "__", "__5f____5f__");

  // Escape all other characters that need to be escaped
  std::string result;
  result.reserve(filename_with_escaped_double_underscores.size());
  for (char c : filename_with_escaped_double_underscores)
  {
    if (IsIllegalCharacter(c))
      result.append(fmt::format("__{:02x}__", c));
    else
      result.push_back(c);
  }

  return result;
}

std::string EscapePath(const std::string& path)
{
  const std::vector<std::string> split_strings = SplitString(path, '/');

  std::vector<std::string> escaped_split_strings;
  escaped_split_strings.reserve(split_strings.size());
  for (const std::string& split_string : split_strings)
    escaped_split_strings.push_back(EscapeFileName(split_string));

  return JoinStrings(escaped_split_strings, "/");
}

std::string UnescapeFileName(const std::string& filename)
{
  std::string result = filename;
  size_t pos = 0;

  // Replace escape sequences of the format "__3f__" with the ASCII
  // character defined by the escape sequence's two hex digits.
  while ((pos = result.find("__", pos)) != std::string::npos)
  {
    u32 character;
    if (pos + 6 <= result.size() && result[pos + 4] == '_' && result[pos + 5] == '_')
      if (AsciiToHex(result.substr(pos + 2, 2), character))
        result.replace(pos, 6, {static_cast<char>(character)});

    ++pos;
  }

  return result;
}

bool IsFileNameSafe(const std::string_view filename)
{
  return !filename.empty() &&
         !std::all_of(filename.begin(), filename.end(), [](char c) { return c == '.'; }) &&
         std::none_of(filename.begin(), filename.end(), IsIllegalCharacter);
}
}  // namespace Common
