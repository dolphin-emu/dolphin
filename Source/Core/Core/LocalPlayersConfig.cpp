// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/LocalPlayersConfig.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace AddPlayers
{
std::vector<AddPlayers> LoadPlayers(const IniFile& localIni)
{
  std::vector<AddPlayers> gcodes;

  for (const IniFile* ini : {&localIni})
  {
    std::vector<std::string> lines;
    ini->GetLines("Local_Players_List", &lines, false);

    AddPlayers gcode;

    for (auto& line : lines)
    {
      std::istringstream ss(line);

      // Some locales (e.g. fr_FR.UTF-8) don't split the string stream on space
      // Use the C locale to workaround this behavior
      ss.imbue(std::locale::classic());

      switch ((line)[0])
      {
      // enabled or disabled code
      case '+':
        if (!gcode.username.empty())
          gcodes.push_back(gcode);
        gcode = AddPlayers();
        ss.seekg(1, std::ios_base::cur);
        // read the code name
        std::getline(ss, gcode.username,
                     '[');  // stop at [ character (beginning of contributor name)
        gcode.username = StripSpaces(gcode.username);
        // read the code creator name
        std::getline(ss, gcode.userid, ']');
        break;

      break;
      }
    }

    // add the last code
    if (!gcode.username.empty())
    {
      gcodes.push_back(gcode);
    }
  }
  return gcodes;
}

static std::string MakePlayerTitle(const AddPlayers& code)
{
  std::string title = '+' + code.username + "[" + code.userid + "]";
  return title;
}


static void SaveLocalPlayers(std::vector<std::string>& lines, const AddPlayers& gcode)
{
  lines.push_back(MakePlayerTitle(gcode));
}

void SavePlayers(IniFile& inifile, const std::vector<AddPlayers>& names)
{
  std::vector<std::string> lines;

  for (const AddPlayers& userName : names)
  {
    SaveLocalPlayers(lines, userName);
  }

  inifile.SetLines("Local_Players_List", lines);
}
}  // namespace AddPlayers
