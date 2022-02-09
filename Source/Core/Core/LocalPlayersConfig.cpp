// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/LocalPlayersConfig.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "Common/MsgHandler.h"

#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace AddPlayers
{
std::vector<AddPlayers> LoadPlayers(const IniFile& localIni)
{
  std::vector<AddPlayers> players;

  for (const IniFile* ini : {&localIni})
  {
    std::vector<std::string> lines;
    ini->GetLines("Local_Players_List", &lines, false);

    AddPlayers player;

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
        if (!player.username.empty())
          players.push_back(player);
        player = AddPlayers();
        ss.seekg(1, std::ios_base::cur);
        // read the code name
        std::getline(ss, player.username,
                     '[');  // stop at [ character (beginning of contributor name)
        player.username = StripSpaces(player.username);
        // read the code creator name
        std::getline(ss, player.userid, ']');
        break;

      break;
      }
    }

    // add the last code
    if (!player.username.empty())
    {
      players.push_back(player);
    }
  }
  return players;
}

static std::string MakePlayerTitle(const AddPlayers& player)
{
  std::string title = '+' + player.username + "[" + player.userid + "]";
  return title;
}


static void SaveLocalPlayers(std::vector<std::string>& lines, const AddPlayers& player)
{
  lines.push_back(MakePlayerTitle(player));
}

void SavePlayers(IniFile& inifile, const std::vector<AddPlayers>& player)
{
  std::vector<std::string> lines;

  for (const AddPlayers& userName : player)
  {
    SaveLocalPlayers(lines, userName);
  }

  inifile.SetLines("Local_Players_List", lines);
}

std::vector<std::string> LoadPortPlayers(IniFile& inifile)
{
  std::vector<std::string> ports;

  IniFile::Section* localplayers = inifile.GetOrCreateSection("Local Players");

  const IniFile::Section::SectionMap portmap = localplayers->GetValues();
  for (const auto& name : portmap)
  {
    ports.push_back(name.second);
  }

  return ports;
}
}  // namespace AddPlayers
