// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 13 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// and to TheFetishMachine and Infernum for encouragement and cheerleading

#include "Core/Config/AchievementSettings.h"

#include <string>

#include "Common/Config/Config.h"
#include "Core/AchievementManager.h"

namespace Config
{
// Configuration Information
const Info<bool> RA_INTEGRATION_ENABLED{{System::Achievements, "General", "IntegrationEnabled"},
                                        false};
const Info<std::string> RA_USERNAME{{System::Achievements, "General", "Username"}, ""};
const Info<std::string> RA_API_TOKEN{{System::Achievements, "General", "ApiToken"}, ""};
const Info<bool> RA_ACHIEVEMENTS_ENABLED{{System::Achievements, "General", "AchievementsEnabled"},
                                         false};
const Info<bool> RA_LEADERBOARDS_ENABLED{{System::Achievements, "General", "LeaderboardsEnabled"},
                                         false};
const Info<bool> RA_RICH_PRESENCE_ENABLED{{System::Achievements, "General", "RichPresenceEnabled"},
                                          false};
const Info<bool> RA_HARDCORE_ENABLED{{System::Achievements, "General", "HardcoreEnabled"}, false};
const Info<bool> RA_BADGE_ICONS_ENABLED{{System::Achievements, "General", "BadgeIconsEnabled"},
                                        false};
const Info<bool> RA_UNOFFICIAL_ENABLED{{System::Achievements, "General", "UnofficialEnabled"},
                                       false};
const Info<bool> RA_ENCORE_ENABLED{{System::Achievements, "General", "EncoreEnabled"}, false};
}  // namespace Config
