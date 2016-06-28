// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/OnionConfig.h"

#include "Core/GeckoCode.h"

namespace Gecko
{
void LoadCodes(OnionConfig::Layer* global_config, OnionConfig::Layer* local_config,
               std::vector<GeckoCode>& gcodes);
void SaveCodes(OnionConfig::Layer* config, const std::vector<GeckoCode>& gcodes);
}
