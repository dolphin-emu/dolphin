// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "Common/Version.h"
#include "Core/Core.h"
#include "DolphinTool/Command.h"
#include "DolphinTool/ConvertCommand.h"
#include "DolphinTool/HeaderCommand.h"
#include "DolphinTool/VerifyCommand.h"

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
  Core::DeclareAsHostThread();

  if (argc < 2)
    return PrintUsage(1);

  const std::string_view command_str = argv[1];
  // Take off the program name and command selector before passing arguments down
  const std::vector<std::string> args(argv + 2, argv + argc);

  std::unique_ptr<DolphinTool::Command> command;

  if (command_str == "convert")
    command = std::make_unique<DolphinTool::ConvertCommand>();
  else if (command_str == "verify")
    command = std::make_unique<DolphinTool::VerifyCommand>();
  else if (command_str == "header")
    command = std::make_unique<DolphinTool::HeaderCommand>();
  else
    return PrintUsage(1);

  return command->Main(args);
}

#ifdef _WIN32
int wmain(int, wchar_t*[], wchar_t*[])
{
  std::vector<std::string> args = Common::CommandLineToUtf8Argv(GetCommandLineW());
  const int argc = static_cast<int>(args.size());
  std::vector<char*> argv(args.size());
  for (size_t i = 0; i < args.size(); ++i)
    argv[i] = args[i].data();

  return main(argc, argv.data());
}

#undef main
#endif
