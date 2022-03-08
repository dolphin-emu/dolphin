// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>
#include "Common/IniFile.h"

#include "Common/CommonTypes.h"

class PointerWrap;

namespace LocalPlayers
{
class LocalPlayers
{
public:
  std::string username, userid;
  std::vector<LocalPlayers> GetPlayers(const IniFile& localIni);

  std::map<int, LocalPlayers> GetPortPlayers(); // port num to the full local player string <name>[<uid>]
  LocalPlayers toLocalPlayer(std::string playerStr);
  std::string LocalPlayerToStr();

  std::string m_local_player_1{"No Player Selected"};
  std::string m_local_player_2{"No Player Selected"};
  std::string m_local_player_3{"No Player Selected"};
  std::string m_local_player_4{"No Player Selected"};
};

}  // namespace LocalPlayers
