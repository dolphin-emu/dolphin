// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
}  // end of namespace MemoryInterface
