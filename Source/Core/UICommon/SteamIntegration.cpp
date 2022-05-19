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
bool IsRelaunchRequired()
{
#ifdef STEAM
  // The Steam API call in this function will use the existence of a file named "steam_appid.txt"
  // to determine if this is a debugging build. If so, then it will not attempt to relaunch Dolphin
  // from the Steam client. This file must be present in the current working directory for this
  // check to succeed, which may not be a valid location for it by default (for example, on macOS,
  // the default current working directory is the root directory, which is read-only).
  // We set the current working directory to the path of the executable or bundle to ensure that
  // the file will always be read from a consistent location.
  File::SetCurrentDir(File::GetExeDirectory());

  // On Steam builds, we should prevent the Dolphin executable from being ran directly. This API
  // will check if Dolphin is running within a Steam context, and if not, will automatically tell
  // the Steam client to launch Dolphin. If that occurs, we should quit this instance of Dolphin.
  const u32 steam_app_id = 1941680;
  if (SteamAPI_RestartAppIfNecessary(steam_app_id))
  {
    return true;
  }
#endif

  return false;
}

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
