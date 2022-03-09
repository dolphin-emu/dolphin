// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>
#include "Core/LocalPlayers.h"

class IniFile;

namespace LocalPlayers
{
  //std::vector<LocalPlayers> LoadPlayers(const IniFile& localIni);
  void SavePlayers(IniFile& inifile, std::vector<LocalPlayers::Player>& players);
  std::vector<std::string> LoadPortPlayers(IniFile& inifile);

  void SaveLocalPorts();
  void LoadLocalPorts();

  extern LocalPlayers::Player m_local_player_1;
  extern LocalPlayers::Player m_local_player_2;
  extern LocalPlayers::Player m_local_player_3;
  extern LocalPlayers::Player m_local_player_4;

  //extern LocalPlayers::Player m_local_player_1{"No Player Selected", "0"};
  //extern LocalPlayers::Player m_local_player_2{"No Player Selected", "0"};
  //extern LocalPlayers::Player m_local_player_3{"No Player Selected", "0"};
  //extern LocalPlayers::Player m_local_player_4{"No Player Selected", "0"};

}  // namespace LocalPlayers


