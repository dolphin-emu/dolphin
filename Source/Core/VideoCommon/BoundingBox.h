// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VertexLoader.h"

// Bounding Box manager

namespace BoundingBox
{

// Determines if bounding box is active
extern bool active;

// Number of consecutive times that bbox regs can be cleared and not used before being disabled
extern int tolerance;

// Bounding box current coordinates
extern u16 coords[4];

enum
{
	LEFT   = 0,
	RIGHT  = 1,
	TOP    = 2,
	BOTTOM = 3
};

// Current position matrix index
extern u8 posMtxIdx;

// Texture matrix indexes
extern u8 texMtxIdx[8];

void LOADERDECL SetVertexBufferPosition();
void LOADERDECL Update();
void Prepare(const VAT & vat, int primitive, const TVtxDesc & vtxDesc, const PortableVertexDeclaration & vtxDecl);

// Save state
void DoState(PointerWrap &p);

}; // end of namespace BoundingBox
