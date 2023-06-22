// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/UISettings.h"

namespace Config
{
// UI.General

const Info<bool> MAIN_USE_DISCORD_PRESENCE{{System::Main, "General", "UseDiscordPresence"}, true};
#ifdef ANDROID
const Info<bool> MAIN_USE_GAME_COVERS{{System::Main, "General", "UseGameCovers"}, true};
#else
const Info<bool> MAIN_USE_GAME_COVERS{{System::Main, "General", "UseGameCovers"}, false};
#endif
const Info<bool> MAIN_FOCUSED_HOTKEYS{{System::Main, "General", "HotkeysRequireFocus"}, true};
const Info<bool> MAIN_RECURSIVE_ISO_PATHS{{System::Main, "General", "RecursiveISOPaths"}, false};

}  // namespace Config
