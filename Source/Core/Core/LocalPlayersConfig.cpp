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
LocalPlayers::Player m_local_player_1{"No Player Selected", "0"};
LocalPlayers::Player m_local_player_2{"No Player Selected", "0"};
LocalPlayers::Player m_local_player_3{"No Player Selected", "0"};
LocalPlayers::Player m_local_player_4{"No Player Selected", "0"};

static void SaveLocalPlayers(std::vector<std::string>& lines, const LocalPlayers::Player& player)
{
  LocalPlayers::Player playerObj = player;
  lines.push_back(playerObj.LocalPlayerToStr());
}

void SavePlayers(IniFile& inifile, std::vector<LocalPlayers::Player>& players)
{
  std::vector<std::string> lines;

  for (unsigned int i = 1; i < players.size(); i++)
  {
    SaveLocalPlayers(lines, players[i]);
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
  const auto ini_path = File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX);
  IniFile ini;
  ini.Load(ini_path);
  IniFile::Section* localplayers = ini.GetOrCreateSection("Local Players");
  localplayers->Set("Player 1", (m_local_player_1).LocalPlayerToStr());
  localplayers->Set("Player 2", (m_local_player_2).LocalPlayerToStr());
  localplayers->Set("Player 3", (m_local_player_3).LocalPlayerToStr());
  localplayers->Set("Player 4", (m_local_player_4).LocalPlayerToStr());
  ini.Save(ini_path);
}

void LoadLocalPorts()
{
  //const auto ini_path = std::string(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  //IniFile local_players_path;
  //local_players_path.Load(ini_path);

  LocalPlayers i_LocalPlayers;
  std::map<int, LocalPlayers::Player> portPlayers = i_LocalPlayers.GetPortPlayers();
  m_local_player_1.SetUserInfo(portPlayers[1]);
  m_local_player_2.SetUserInfo(portPlayers[2]);
  m_local_player_3.SetUserInfo(portPlayers[3]);
  m_local_player_4.SetUserInfo(portPlayers[4]);
}

}  // namespace LocalPlayers
