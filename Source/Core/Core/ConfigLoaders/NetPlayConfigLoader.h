// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Config.h"
#include "Core/NetPlayProto.h"

Config::ConfigLayerLoader* GenerateNetPlayConfigLoader(const NetSettings& settings);
