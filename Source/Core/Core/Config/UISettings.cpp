// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included

#include "Core/Config/UISettings.h"

namespace Config
{
// UI.General

const Info<bool> MAIN_USE_DISCORD_PRESENCE{{System::Main, "General", "UseDiscordPresence"}, true};
const Info<bool> MAIN_USE_GAME_COVERS{{System::Main, "General", "UseGameCovers"}, false};
const Info<bool> MAIN_FOCUSED_HOTKEYS{{System::Main, "General", "HotkeysRequireFocus"}, true};
const Info<bool> MAIN_RECURSIVE_ISO_PATHS{{System::Main, "General", "RecursiveISOPaths"}, false};

// UI.Android

const Info<int> MAIN_LAST_PLATFORM_TAB{{System::Main, "Android", "LastPlatformTab"}, 0};

}  // namespace Config
