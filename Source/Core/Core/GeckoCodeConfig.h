// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "Common/Config.h"
#include "Core/GeckoCode.h"

namespace Gecko
{
void LoadCodes(Config::Layer& globalIni, Config::Layer& localIni, std::vector<GeckoCode>& gcodes);
void SaveCodes(Config::Layer& config, const std::vector<GeckoCode>& gcodes);
}
