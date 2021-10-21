// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Config.h"

// This is a temporary soluation, although they should be in their repected cpp file in UICommon.
// However, in order for IsSettingSaveable to compile without some issues, this needs to be here.
// Once IsSettingSaveable is removed, then you should able to move these back to UICommon.

namespace Config
{
// Configuration Information

// UI.General

extern const Info<bool> MAIN_USE_DISCORD_PRESENCE;
extern const Info<bool> MAIN_USE_GAME_COVERS;
extern const Info<bool> MAIN_FOCUSED_HOTKEYS;
extern const Info<bool> MAIN_RECURSIVE_ISO_PATHS;

}  // namespace Config
