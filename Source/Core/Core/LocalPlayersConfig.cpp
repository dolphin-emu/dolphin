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
#include "Common/FileUtil.h"

namespace LocalPlayers
{
//std::vector<LocalPlayers> LoadPlayers(const IniFile& localIni)
//{
//  LocalPlayers player;
//  return player.GetPlayers(localIni);
//}


static void SaveLocalPlayers(std::vector<std::string>& lines, const LocalPlayers& player)
{
  LocalPlayers playerObj = player;
  lines.push_back(playerObj.LocalPlayerToStr());
}

void SavePlayers(IniFile& inifile, const std::vector<LocalPlayers>& players)
{
  std::vector<std::string> lines;

  for (const LocalPlayers& player : players)
  {
    SaveLocalPlayers(lines, player);
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

void SaveLocalPorts()
{
  LocalPlayers i_LocalPlayers;
  const auto ini_path = File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX);
  IniFile ini;
  ini.Load(ini_path);
  IniFile::Section* localplayers = ini.GetOrCreateSection("Local Players");
  localplayers->Set("Player 1", i_LocalPlayers.m_local_player_1);
  localplayers->Set("Player 2", i_LocalPlayers.m_local_player_2);
  localplayers->Set("Player 3", i_LocalPlayers.m_local_player_3);
  localplayers->Set("Player 4", i_LocalPlayers.m_local_player_4);
  ini.Save(ini_path);
}

void LoadLocalPorts()
{
  //const auto ini_path = std::string(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  //IniFile local_players_path;
  //local_players_path.Load(ini_path);

  LocalPlayers i_LocalPlayers;
  std::map<int, LocalPlayers> portPlayers = i_LocalPlayers.GetPortPlayers();
  i_LocalPlayers.m_local_player_1 = portPlayers[0].LocalPlayerToStr();
  i_LocalPlayers.m_local_player_2 = portPlayers[0].LocalPlayerToStr();
  i_LocalPlayers.m_local_player_3 = portPlayers[0].LocalPlayerToStr();
  i_LocalPlayers.m_local_player_4 = portPlayers[0].LocalPlayerToStr();
}


}  // namespace LocalPlayers
