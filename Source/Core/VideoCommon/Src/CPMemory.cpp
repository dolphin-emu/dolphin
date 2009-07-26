// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"
#include "CPMemory.h"

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
