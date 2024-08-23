// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS

#include "Common/Config/Config.h"

namespace Config
{
// Configuration Information
extern const Info<bool> RA_ENABLED;
extern const Info<std::string> RA_USERNAME;
extern const Info<std::string> RA_HOST_URL;
extern const Info<std::string> RA_API_TOKEN;
extern const Info<bool> RA_HARDCORE_ENABLED;
extern const Info<bool> RA_UNOFFICIAL_ENABLED;
extern const Info<bool> RA_ENCORE_ENABLED;
extern const Info<bool> RA_SPECTATOR_ENABLED;
extern const Info<bool> RA_DISCORD_PRESENCE_ENABLED;
extern const Info<bool> RA_PROGRESS_ENABLED;
}  // namespace Config

#endif  // USE_RETRO_ACHIEVEMENTS
