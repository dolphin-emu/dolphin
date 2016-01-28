// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/OnionConfig.h"
#include "Core/NetPlayProto.h"

OnionConfig::ConfigLayerLoader* GenerateNetPlayConfigLoader(const NetSettings& settings);
