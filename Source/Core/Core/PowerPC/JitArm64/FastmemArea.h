// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

struct FastmemArea
{
  u8* fast_access_start;
  u8* fast_access_end;
  const u8* slow_access_code;
};
