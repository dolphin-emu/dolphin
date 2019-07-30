// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
