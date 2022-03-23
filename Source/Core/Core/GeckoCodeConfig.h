// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Core/GeckoCode.h"

class IniFile;

namespace Gecko
{
GeckoGameConfig LoadGameConfig(const IniFile& globalIni, const IniFile& localIni);
std::vector<std::string> LoadGameConfigTxt(const IniFile& globalIni, const IniFile& localIni);
std::vector<GeckoCode> LoadCodes(const IniFile& globalIni, const IniFile& localIni);
std::vector<GeckoCode> DownloadCodes(std::string gametdb_id, bool* succeeded,
                                     bool use_https = true);
void SaveCodes(IniFile& inifile, const std::vector<GeckoCode>& gcodes,
               const std::vector<std::string> ggameconfig);

std::optional<GeckoCode::Code> DeserializeLine(const std::string& line);
}  // namespace Gecko
