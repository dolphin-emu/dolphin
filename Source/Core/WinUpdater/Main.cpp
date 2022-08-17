// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <ShlObj.h>
#include <shellapi.h>

#include <optional>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
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

  std::vector<std::string> args = CommandLineToUtf8Argv(pCmdLine);
  std::optional<Options> maybe_opts = ParseCommandLine(args);

  if (!maybe_opts)
  {
    return false;
  }
  const Options options = std::move(*maybe_opts);

  const std::string test_file_path = options.install_base_path + DIR_SEP_CHR + "test.tmp";
  const bool file_creation_failed = !File::CreateEmptyFile(test_file_path);
  const bool file_deletion_failed = !File::Delete(test_file_path);
  const bool need_admin = file_creation_failed || file_deletion_failed;

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

  return RunUpdater(options) ? 0 : 1;
}
