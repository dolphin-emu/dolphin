// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/IniFile.h"

template <typename T>
void ReadEnabledOrDisabled(const Common::IniFile& ini, const std::string& section, bool enabled,
                           std::vector<T>* codes)
{
  std::vector<std::string> lines;
  ini.GetLines(section, &lines, false);

  for (const std::string& line : lines)
  {
    if (line.empty() || line[0] != '$')
      continue;

    for (T& code : *codes)
    {
      // Exclude the initial '$' from the comparison.
      if (line.compare(1, std::string::npos, code.name) == 0)
        code.enabled = enabled;
    }
  }
}

template <typename T>
void ReadEnabledAndDisabled(const Common::IniFile& ini, const std::string& section,
                            std::vector<T>* codes)
{
  ReadEnabledOrDisabled(ini, section + "_Enabled", true, codes);
  ReadEnabledOrDisabled(ini, section + "_Disabled", false, codes);
}
