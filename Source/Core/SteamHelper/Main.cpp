// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#include <thread>

#include <steam/steam_api.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "SteamHelperCommon/Constants.h"

#include "SteamHelper/HelperServer.h"

int main(int, char*[])
{
#ifdef _WIN32
  if (!GetEnvironmentVariableA(STEAM_HELPER_ENV_VAR_NAME, nullptr, 0))
  {
    MessageBoxW(
        nullptr,
        L"This application is not meant to be launched directly. Run Dolphin from Steam instead.",
        L"Error", MB_ICONERROR);
    return 1;
  }
#else
  if (getenv(STEAM_HELPER_ENV_VAR_NAME) == nullptr)
  {
#ifdef __APPLE__
    CFUserNotificationDisplayAlert(0, kCFUserNotificationStopAlertLevel, nullptr, nullptr, nullptr,
                                   CFSTR("Error"),
                                   CFSTR("This application is not meant to be launched directly. "
                                         "Run Dolphin from Steam instead."),
                                   nullptr, nullptr, nullptr, nullptr);
#else
    fprintf(stderr, "error: This application is not meant to be launched directly. Run Dolphin "
                    "from Steam instead.\n");
#endif

    return 1;
  }
#endif

#ifdef _WIN32
  PipeHandle in_handle = GetStdHandle(STD_INPUT_HANDLE);
  PipeHandle out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
#else
  PipeHandle in_handle = STDIN_FILENO;
  PipeHandle out_handle = STDOUT_FILENO;
#endif

  Steam::HelperServer server(in_handle, out_handle);

  while (server.IsRunning())
  {
    if (server.GetSteamInited())
    {
      SteamAPI_RunCallbacks();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // run at 10 Hz
  }

  SteamAPI_Shutdown();

  return 0;
}
