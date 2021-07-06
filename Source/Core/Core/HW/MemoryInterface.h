// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace MMIO
{
class Mapping;
}
class PointerWrap;

namespace MemoryInterface
{
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);
}  // namespace MemoryInterface
