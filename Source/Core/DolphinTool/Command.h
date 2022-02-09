// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

namespace DolphinTool
{
class Command
{
public:
  Command() {}
  virtual ~Command() {}
  virtual int Main(const std::vector<std::string>& args) = 0;
};

}  // namespace DolphinTool
