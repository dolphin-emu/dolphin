// Copyright (C) 2003 Dolphin Project.

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

#include "EFBCopy.h"

const unsigned int EFB_COPY_BLOCK_WIDTHS[16] = {
	8, // R4
	8, // R8_1
	8, // RA4
	4, // RA8
	4, // RGB565
	4, // RGB5A3
	4, // RGBA8
	8, // A8
	8, // R8
	8, // G8
	8, // B8
	4, // RG8
	4, // GB8
	0, 0, 0 // Unknown formats
};

const unsigned int EFB_COPY_BLOCK_HEIGHTS[16] = {
	8, // R4
	4, // R8_1
	4, // RA4
	4, // RA8
	4, // RGB565
	4, // RGB5A3
	4, // RGBA8
	4, // A8
	4, // R8
	4, // G8
	4, // B8
	4, // RG8
	4, // GB8
	0, 0, 0 // Unknown formats
};
