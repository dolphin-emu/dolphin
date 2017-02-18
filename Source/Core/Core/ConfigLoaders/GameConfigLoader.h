// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Config.h"

Config::ConfigLayerLoader* GenerateGlobalGameConfigLoader(const std::string& id, u16 revision);
Config::ConfigLayerLoader* GenerateLocalGameConfigLoader(const std::string& id, u16 revision);
