// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included

#include "Core/Config/UISettings.h"

namespace Config
{
// UI.General

const ConfigInfo<bool> MAIN_USE_DISCORD_PRESENCE{{System::Main, "General", "UseDiscordPresence"},
                                                 true};
const ConfigInfo<bool> MAIN_USE_GAME_COVERS{{System::Main, "General", "UseGameCovers"}, false};

}  // namespace Config
