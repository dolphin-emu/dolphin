// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Config/Config.h"

namespace Discord
{
void Init();
void UpdateDiscordPresence();
void Shutdown();
void SetDiscordPresenceEnabled(bool enabled);
}  // namespace Discord

extern const Config::ConfigInfo<bool> MAIN_USE_DISCORD_PRESENCE;
