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

}  // namespace Config
