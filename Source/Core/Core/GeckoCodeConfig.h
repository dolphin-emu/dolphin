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
std::vector<GeckoCode> LoadCodes(const IniFile& globalIni, const IniFile& localIni);
void SaveCodes(IniFile& inifile, const std::vector<GeckoCode>& gcodes);

std::optional<GeckoCode::Code> DeserializeLine(const std::string& line);
}  // namespace Gecko
