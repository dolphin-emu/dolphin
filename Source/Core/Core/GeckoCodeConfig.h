// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Core/GeckoCode.h"

namespace Common
{
class IniFile;
}

namespace Gecko
{
std::vector<GeckoCode> LoadCodes(const Common::IniFile& globalIni, const Common::IniFile& localIni);
std::expected<std::vector<GeckoCode>, int> DownloadCodes(std::string_view gametdb_id);
void SaveCodes(Common::IniFile& inifile, std::span<const GeckoCode> gcodes);

std::optional<GeckoCode::Code> DeserializeLine(const std::string& line);
}  // namespace Gecko
