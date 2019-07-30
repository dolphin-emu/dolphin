// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <windows.h>
#include <ShlObj.h>
#include <shellapi.h>

#include <optional>
#include <string>
#include <vector>

#include "Common/StringUtil.h"

#include "UpdaterCommon/UI.h"
#include "UpdaterCommon/UpdaterCommon.h"

namespace
{
std::vector<std::string> CommandLineToUtf8Argv(PCWSTR command_line)
{
  int nargs;
  LPWSTR* tokenized = CommandLineToArgvW(command_line, &nargs);
  if (!tokenized)
    return {};

  std::vector<std::string> argv(nargs);
  for (int i = 0; i < nargs; ++i)
  {
    argv[i] = UTF16ToUTF8(tokenized[i]);
  }

  LocalFree(tokenized);
  return argv;
}

std::optional<std::wstring> GetModuleName(HINSTANCE hInstance)
{
  std::wstring name;
  DWORD max_size = 50;  // Start with space for 50 characters and grow if needed
  name.resize(max_size);

  DWORD size;
  while ((size = GetModuleFileNameW(hInstance, name.data(), max_size)) == max_size &&
         GetLastError() == ERROR_INSUFFICIENT_BUFFER)
  {
    max_size *= 2;
    name.resize(max_size);
  }

  if (size == 0)
  {
    return {};
  }
  name.resize(size);
  return name;
}
};  // namespace

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
