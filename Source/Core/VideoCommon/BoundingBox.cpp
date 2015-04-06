// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.



#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SetupUnit.h"
#include "VideoBackends/Software/TransformUnit.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/PixelShaderManager.h"


namespace BoundingBox
{

// External vars
bool active = false;
u16 coords[4] = { 0x80, 0xA0, 0x80, 0xA0 };

// Save state
void DoState(PointerWrap &p)
{
	p.Do(active);
	p.Do(coords);
}

} // namespace BoundingBox
