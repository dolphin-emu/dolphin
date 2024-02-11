// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/FBInfo.h"

#include "Common/Hash.h"

u32 FBInfo::CalculateHash() const
{
  return Common::HashAdler32(reinterpret_cast<const u8*>(this), sizeof(FBInfo));
}
