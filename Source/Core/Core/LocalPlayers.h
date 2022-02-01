// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace AddPlayers
{
class AddPlayers
{
public:
  std::string username, userid;

  bool Exist(u32 address, u32 data) const;
};
}  // namespace Gecko
