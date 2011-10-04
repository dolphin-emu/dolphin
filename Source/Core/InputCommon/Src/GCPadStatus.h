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

#ifndef _GCPAD_H_INCLUDED__
#define _GCPAD_H_INCLUDED__

#include "CommonTypes.h"

#define PAD_ERR_NONE            0
#define PAD_ERR_NO_CONTROLLER   -1
#define PAD_ERR_NOT_READY       -2
#define PAD_ERR_TRANSFER        -3

#define PAD_USE_ORIGIN          0x0080

#define PAD_BUTTON_LEFT         0x0001
#define PAD_BUTTON_RIGHT        0x0002
#define PAD_BUTTON_DOWN         0x0004
#define PAD_BUTTON_UP           0x0008
#define PAD_TRIGGER_Z           0x0010
#define PAD_TRIGGER_R           0x0020
#define PAD_TRIGGER_L           0x0040
#define PAD_BUTTON_A            0x0100
#define PAD_BUTTON_B            0x0200
#define PAD_BUTTON_X            0x0400
#define PAD_BUTTON_Y            0x0800
#define PAD_BUTTON_START        0x1000

typedef struct
{
	unsigned short	button;                 // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
	unsigned char	stickX;                 // 0 <= stickX       <= 255
	unsigned char	stickY;                 // 0 <= stickY       <= 255
	unsigned char	substickX;              // 0 <= substickX    <= 255
	unsigned char	substickY;              // 0 <= substickY    <= 255
	unsigned char	triggerLeft;            // 0 <= triggerLeft  <= 255
	unsigned char	triggerRight;           // 0 <= triggerRight <= 255
	unsigned char	analogA;                // 0 <= analogA      <= 255
	unsigned char	analogB;                // 0 <= analogB      <= 255
	signed char		err;                    // one of PAD_ERR_* number
} SPADStatus;

#endif
