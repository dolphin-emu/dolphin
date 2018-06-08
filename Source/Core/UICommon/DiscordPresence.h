// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Discord
{
void Init();
void UpdateDiscordPresence();
void Shutdown();
void SetDiscordPresenceEnabled(bool enabled);
}  // namespace Discord
