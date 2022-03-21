// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "DolphinTool/Command.h"

namespace DolphinTool
{
class HeaderCommand final : public Command
{
public:
  int Main(const std::vector<std::string>& args) override;
};

}  // namespace DolphinTool
