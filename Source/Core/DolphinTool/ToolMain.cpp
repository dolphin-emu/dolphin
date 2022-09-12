// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "Common/Version.h"

import ConvertCommand;
import HeaderCommand;
import VerifyCommand;

static int PrintUsage(int code)
{
  std::cerr << "usage: dolphin-tool COMMAND -h" << std::endl << std::endl;
  std::cerr << "commands supported: [convert, verify, header]" << std::endl;

  return code;
}

#ifdef _WIN32
#define main app_main
#endif

int main(int argc, char* argv[])
{
  if (argc < 2)
    return PrintUsage(1);

  std::vector<std::string> args(argv, argv + argc);

  std::string command_str = args.at(1);

  // Take off the command selector before passing arguments down
  args.erase(args.begin(), args.begin() + 1);

  if (command_str == "convert")
    return DolphinTool::ConvertCommand::Main(args);
  else if (command_str == "verify")
    return DolphinTool::VerifyCommand::Main(args);
  else if (command_str == "header")
    return DolphinTool::HeaderCommand::Main(args);
  else
    return PrintUsage(1);
}

#ifdef _WIN32
int wmain(int, wchar_t*[], wchar_t*[])
{
  std::vector<std::string> args = CommandLineToUtf8Argv(GetCommandLineW());
  const int argc = static_cast<int>(args.size());
  std::vector<char*> argv(args.size());
  for (size_t i = 0; i < args.size(); ++i)
    argv[i] = args[i].data();

  return main(argc, argv.data());
}

#undef main
#endif
