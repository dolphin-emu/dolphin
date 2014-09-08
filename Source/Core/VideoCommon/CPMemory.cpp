// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"

// CP state
u8 *cached_arraybases[16];

// STATE_TO_SAVE
u32 arraybases[16];
u32 arraystrides[16];
TMatrixIndexA MatrixIndexA;
TMatrixIndexB MatrixIndexB;
TVtxDesc g_VtxDesc;
// Most games only use the first VtxAttr and simply reconfigure it all the time as needed.
VAT g_VtxAttr[8];
