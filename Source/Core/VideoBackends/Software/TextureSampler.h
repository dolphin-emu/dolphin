// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace TextureSampler
{
void Sample(s32 s, s32 t, s32 lod, bool linear, u8 texmap, u8* sample);

void SampleMip(s32 s, s32 t, s32 mip, bool linear, u8 texmap, u8* sample);

enum
{
  RED_SMP,
  GRN_SMP,
  BLU_SMP,
  ALP_SMP
};
}  // namespace TextureSampler
