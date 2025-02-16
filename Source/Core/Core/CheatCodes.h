// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <regex>
#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "Common/StringUtil.h"

template <typename T, bool disregard_creator_in_name = false>
void ReadEnabledOrDisabled(const Common::IniFile& ini, const std::string& section, bool enabled,
                           std::vector<T>* codes)
{
  std::vector<std::string> lines;
  ini.GetLines(section, &lines, false);

  for (const std::string& line : lines)
  {
    if (line.empty() || line[0] != '$')
      continue;

    // Exclude the initial '$' from the comparison.
    const auto name = StripWhitespace(std::string_view{line}.substr(1));

    bool matched{false};

    for (T& code : *codes)
    {
      if (name == code.name)
      {
        code.enabled = enabled;
        matched = true;
        break;
      }
    }

    if (!matched && disregard_creator_in_name)
    {
      // For backwards compatibility, where certain cheat code types (namedly Gecko cheat codes)
      // would be stored in the _Enabled/_Disabled sections without including the creator name,
      // there will be a second attempt to match the parsed line with any of the cheat codes in the
      // list after disregarding the potential creator name.
      static const std::regex s_regex("(.*)(\\[.+\\])");
      for (T& code : *codes)
      {
        const std::string codeNameWithoutCreator{
            StripWhitespace(std::regex_replace(code.name, s_regex, "$1"))};

        if (name == codeNameWithoutCreator)
        {
          code.enabled = enabled;
          matched = true;
          break;
        }
      }
    }
  }
}

template <typename T, bool disregard_creator_in_name = false>
void ReadEnabledAndDisabled(const Common::IniFile& ini, const std::string& section,
                            std::vector<T>* codes)
{
  ReadEnabledOrDisabled<T, disregard_creator_in_name>(ini, section + "_Enabled", true, codes);
  ReadEnabledOrDisabled<T, disregard_creator_in_name>(ini, section + "_Disabled", false, codes);
}
