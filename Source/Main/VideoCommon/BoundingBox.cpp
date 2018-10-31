// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/BoundingBox.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

namespace BoundingBox
{
// External vars
bool active = false;
u16 coords[4] = {0x80, 0xA0, 0x80, 0xA0};

// Save state
void DoState(PointerWrap& p)
{
  p.Do(active);
  p.Do(coords);
}

}  // namespace BoundingBox
