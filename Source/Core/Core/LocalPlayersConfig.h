// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>
#include "Core/LocalPlayers.h"

class IniFile;

namespace AddPlayers
{
  std::vector<AddPlayers> LoadPlayers(const IniFile& localIni);
  void SavePlayers(IniFile& inifile, const std::vector<AddPlayers>& players);
  std::vector<std::string> LoadPortPlayers(IniFile& inifile);
  }  // namespace AddPlayers
