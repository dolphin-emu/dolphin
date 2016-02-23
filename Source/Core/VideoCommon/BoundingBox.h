// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;

// Bounding Box manager
namespace BoundingBox
{

// Determines if bounding box is active
extern bool active;

// Bounding box current coordinates
extern u16 coords[4];

enum
{
	LEFT   = 0,
	RIGHT  = 1,
	TOP    = 2,
	BOTTOM = 3
};

// Save state
void DoState(PointerWrap& p);

}; // end of namespace BoundingBox
