// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

constexpr auto WII_STATE_FILE = "state.dat";
constexpr auto WII_SYSCONF_DIR = "shared2" DIR_SEP "sys";
constexpr auto WII_WC24CONF_DIR = "shared2" DIR_SEP "wc24";

namespace Common
{
std::string RootUserPath(FromWhichRoot from)
{
  return from == FROM_CONFIGURED_ROOT ? Paths::GetWiiRootDir() : Paths::GetSessionWiiRootDir();
}

std::string GetSysconfDir(FromWhichRoot from)
{
  return RootUserPath(from) + DIR_SEP + WII_SYSCONF_DIR + DIR_SEP;
}

std::string GetWC24ConfDir(FromWhichRoot from)
{
  return RootUserPath(from) + WII_WC24CONF_DIR;
}

std::string GetImportTitlePath(u64 title_id, FromWhichRoot from)
{
  return RootUserPath(from) + StringFromFormat("/import/%08x/%08x",
                                               static_cast<u32>(title_id >> 32),
                                               static_cast<u32>(title_id));
}

std::string GetTicketFileName(u64 _titleID, FromWhichRoot from)
{
  return StringFromFormat("%s/ticket/%08x/%08x.tik", RootUserPath(from).c_str(),
                          (u32)(_titleID >> 32), (u32)_titleID);
}

std::string GetTitleDataPath(u64 _titleID, FromWhichRoot from)
{
  return StringFromFormat("%s/title/%08x/%08x/data/", RootUserPath(from).c_str(),
                          (u32)(_titleID >> 32), (u32)_titleID);
}

std::string GetTitleStateFileName(u64 titleID, FromWhichRoot from)
{
  return GetTitleDataPath(titleID, from) + WII_STATE_FILE;
}

std::string GetTMDFileName(u64 _titleID, FromWhichRoot from)
{
  return GetTitleContentPath(_titleID, from) + "title.tmd";
}
std::string GetTitleContentPath(u64 _titleID, FromWhichRoot from)
{
  return StringFromFormat("%s/title/%08x/%08x/content/", RootUserPath(from).c_str(),
                          (u32)(_titleID >> 32), (u32)_titleID);
}

std::string EscapeFileName(const std::string& filename)
{
  // Prevent paths from containing special names like ., .., ..., ...., and so on
  if (std::all_of(filename.begin(), filename.end(), [](char c) { return c == '.'; }))
    return ReplaceAll(filename, ".", "__2e__");

  // Escape all double underscores since we will use double underscores for our escape sequences
  std::string filename_with_escaped_double_underscores = ReplaceAll(filename, "__", "__5f____5f__");

  // Escape all other characters that need to be escaped
  static const std::unordered_set<char> chars_to_replace = {'\"', '*', '/',  ':', '<',
                                                            '>',  '?', '\\', '|', '\x7f'};
  std::string result;
  result.reserve(filename_with_escaped_double_underscores.size());
  for (char c : filename_with_escaped_double_underscores)
  {
    if ((c >= 0 && c <= 0x1F) || chars_to_replace.find(c) != chars_to_replace.end())
      result.append(StringFromFormat("__%02x__", c));
    else
      result.push_back(c);
  }

  return result;
}

std::string EscapePath(const std::string& path)
{
  std::vector<std::string> split_strings;
  SplitString(path, '/', split_strings);

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
}
