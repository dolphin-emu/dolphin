// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <ShlObj.h>
#include <shellapi.h>

#include <optional>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/StringUtil.h"

#include "UpdaterCommon/UI.h"
#include "UpdaterCommon/UpdaterCommon.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
  if (lstrlenW(pCmdLine) == 0)
  {
    MessageBox(nullptr,
               L"This updater is not meant to be launched directly. Configure Auto-Update in "
               "Dolphin's settings instead.",
               L"Error", MB_ICONERROR);
    return 1;
  }

  // Test for write permissions
  bool need_admin = false;

  FILE* test_fh = fopen("Updater.log", "w");

  if (test_fh == nullptr)
    need_admin = true;
  else
    fclose(test_fh);

  if (need_admin)
  {
    if (IsUserAnAdmin())
    {
      MessageBox(nullptr, L"Failed to write to directory despite administrator priviliges.",
                 L"Error", MB_ICONERROR);
      return 1;
    }

    auto path = GetModuleName(hInstance);
    if (!path)
    {
      MessageBox(nullptr, L"Failed to get updater filename.", L"Error", MB_ICONERROR);
      return 1;
    }

    // Relaunch the updater as administrator
    ShellExecuteW(nullptr, L"runas", path->c_str(), pCmdLine, NULL, SW_SHOW);
    return 0;
  }

  std::vector<std::string> args = CommandLineToUtf8Argv(pCmdLine);

  return RunUpdater(args) ? 0 : 1;
}
