// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Config/Config.h"

// This is a temporary soluation, although they should be in their repected cpp file in UICommon.
// However, in order for IsSettingSaveable to commpile without some issues, this needs to be here.
// Once IsSettingSaveable is removed, then you should able to move these back to UICommon.

namespace Config
{
// Configuration Information

// UI.General

extern const ConfigInfo<bool> MAIN_USE_DISCORD_PRESENCE;
extern const ConfigInfo<bool> MAIN_USE_GAME_COVERS;

}  // namespace Config
