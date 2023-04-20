// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Config.h"

namespace Config
{
// Configuration Information
extern const Info<bool> RA_ENABLED;
extern const Info<std::string> RA_USERNAME;
extern const Info<std::string> RA_API_TOKEN;
extern const Info<bool> RA_ACHIEVEMENTS_ENABLED;
extern const Info<bool> RA_LEADERBOARDS_ENABLED;
extern const Info<bool> RA_RICH_PRESENCE_ENABLED;
extern const Info<bool> RA_UNOFFICIAL_ENABLED;
extern const Info<bool> RA_ENCORE_ENABLED;
}  // namespace Config
