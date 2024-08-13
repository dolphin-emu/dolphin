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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nShowCmd)
{
  if (lstrlenW(lpCmdLine) == 0)
  {
    MessageBoxW(nullptr,
                L"This updater is not meant to be launched directly. Configure Auto-Update in "
                "Dolphin's settings instead.",
                L"Error", MB_ICONERROR);
    return 1;
  }

  // Test for write permissions
  bool need_admin = false;

  auto path = Common::GetModuleName(hInstance);
  if (!path)
  {
    UI::Error("Failed to get updater filename.");
    return 1;
  }

  const auto test_fh = ::CreateFileW(
      (std::filesystem::path(*path).parent_path() / "directory_writable_check.tmp").c_str(),
      GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);

  if (test_fh == INVALID_HANDLE_VALUE)
    need_admin = true;
  else
    CloseHandle(test_fh);

  if (need_admin)
  {
    if (IsUserAnAdmin())
    {
      MessageBox(nullptr, L"Failed to write to directory despite administrator priviliges.",
                 L"Error", MB_ICONERROR);
      return 1;
    }

    // Relaunch the updater as administrator
    ShellExecuteW(nullptr, L"runas", path->c_str(), lpCmdLine, nullptr, SW_SHOW);
    return 0;
  }

  std::vector<std::string> args = Common::CommandLineToUtf8Argv(lpCmdLine);

  return RunUpdater(args) ? 0 : 1;
}
