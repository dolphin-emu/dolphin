// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>
#include "Core/GeckoCode.h"

class IniFile;

namespace Gecko
{
std::vector<GeckoCode> LoadCodes(const IniFile& globalIni, const IniFile& localIni);
std::vector<GeckoCode> DownloadCodes(std::string gametdb_id, bool* succeeded);
void SaveCodes(IniFile& inifile, const std::vector<GeckoCode>& gcodes);
}  // namespace Gecko
