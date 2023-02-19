// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 13 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// and to TheFetishMachine and Infernum for encouragement and cheerleading

#pragma once

#include "Common/Config/Config.h"

namespace Config
{
// Configuration Information
extern const Info<bool> RA_INTEGRATION_ENABLED;
extern const Info<std::string> RA_USERNAME;
extern const Info<std::string> RA_API_TOKEN;
extern const Info<bool> RA_ACHIEVEMENTS_ENABLED;
extern const Info<bool> RA_LEADERBOARDS_ENABLED;
extern const Info<bool> RA_RICH_PRESENCE_ENABLED;
extern const Info<bool> RA_HARDCORE_ENABLED;
extern const Info<bool> RA_BADGE_ICONS_ENABLED;
extern const Info<bool> RA_UNOFFICIAL_ENABLED;
extern const Info<bool> RA_ENCORE_ENABLED;
}  // namespace Config
