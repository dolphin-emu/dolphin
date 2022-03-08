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
  void SavePlayers(IniFile& inifile, const std::vector<LocalPlayers>& players);
  std::vector<std::string> LoadPortPlayers(IniFile& inifile);

  void SaveLocalPorts();
  void LoadLocalPorts();

}  // namespace LocalPlayers
