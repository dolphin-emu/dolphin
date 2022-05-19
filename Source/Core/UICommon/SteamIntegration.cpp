// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/SteamIntegration.h"

#ifdef STEAM

#include <steam/steam_api.h>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"

#endif

namespace UICommon::Steam
{
void Init()
{
#ifdef STEAM
  if (!SteamAPI_Init())
  {
    PanicAlertFmt("Failed to initialize Steam API!");
  }
#endif
}

void Shutdown()
{
#ifdef STEAM
  SteamAPI_Shutdown();
#endif
}

}  // namespace Steam
