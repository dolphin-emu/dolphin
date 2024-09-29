// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "Core/Config/AchievementSettings.h"

#include <string>

#include "Common/Config/Config.h"

namespace Config
{
// Configuration Information
const Info<bool> RA_ENABLED{{System::Achievements, "Achievements", "Enabled"}, false};
const Info<std::string> RA_HOST_URL{{System::Achievements, "Achievements", "HostUrl"}, ""};
const Info<std::string> RA_USERNAME{{System::Achievements, "Achievements", "Username"}, ""};
const Info<std::string> RA_API_TOKEN{{System::Achievements, "Achievements", "ApiToken"}, ""};
const Info<bool> RA_HARDCORE_ENABLED{{System::Achievements, "Achievements", "HardcoreEnabled"},
                                     true};
const Info<bool> RA_UNOFFICIAL_ENABLED{{System::Achievements, "Achievements", "UnofficialEnabled"},
                                       false};
const Info<bool> RA_ENCORE_ENABLED{{System::Achievements, "Achievements", "EncoreEnabled"}, false};
const Info<bool> RA_SPECTATOR_ENABLED{{System::Achievements, "Achievements", "SpectatorEnabled"},
                                      false};
const Info<bool> RA_DISCORD_PRESENCE_ENABLED{
    {System::Achievements, "Achievements", "DiscordPresenceEnabled"}, false};
const Info<bool> RA_PROGRESS_ENABLED{{System::Achievements, "Achievements", "ProgressEnabled"},
                                     false};
}  // namespace Config

#endif  // USE_RETRO_ACHIEVEMENTS
