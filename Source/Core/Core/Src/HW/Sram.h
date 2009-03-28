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

// Modified code taken from libogc
/*-------------------------------------------------------------

system.h -- OS functions and initialization

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.


-------------------------------------------------------------*/
#ifndef __SRAM_h__
#define __SRAM_h__

#include "Common.h"

#pragma pack(push,1)
union SRAM
{
	u8 p_SRAM[64];
	struct _syssram {			// Stored configuration value from the system SRAM area
		u8 checksum[2];			// holds the block checksum.
		u8 checksum_inv[2];		// holds the inverse block checksum
		u8 ead0[4];				// unknown attribute
		u8 ead1[4];				// unknown attribute
		u8 counter_bias[4];		// bias value for the realtime clock
		s8 display_offsetH;		// pixel offset for the VI
		u8 ntd;					// unknown attribute
		u8 lang;				// language of system
		u8 flags;				// device and operations flag

								// Stored configuration value from the extended SRAM area
		u8 flash_id[2][12];		// flash_id[2][12] 96bit memorycard unlock flash ID
		u8 wirelessKbd_id[4];	// Device ID of last connected wireless keyboard
		u8 wirelessPad_id[8];	// 16bit device ID of last connected pad.
		u8 dvderr_code;			// last non-recoverable error from DVD interface
		u8 __padding0;			// padding
		u8 flashID_chksum[4];	// 16bit checksum of unlock flash ID
		u8 __padding1[2];		// padding - libogc has this as [4]? I have it as 2 to make it 64
	}syssram;
};
#pragma pack(pop)
#endif
