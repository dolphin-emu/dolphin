// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


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

1. The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.


-------------------------------------------------------------*/
#pragma once

#include "Common/CommonTypes.h"

#pragma pack(push,1)
union SRAMFlags
{
	u8 Hex;
	struct
	{
		u8             : 2;
		u8 sound       : 1; // Audio settings; 0 = Mono, 1 = Stereo
		u8 initialized : 1; // if 0, displays prompt to set language on boot and asks user to set options and time/date
		u8             : 2;
		u8 boot_menu   : 1; // if 1, skips logo animation and boots into the system menu regardless of if there is a disc inserted
		u8 progressive : 1; // if 1, automatically displays Progressive Scan prompt in games that support it
	};
};

union SRAM
{
	u8 p_SRAM[64];
	struct                      // Stored configuration value from the system SRAM area
	{
		u16 checksum;           // Holds the block checksum.
		u16 checksum_inv;       // Holds the inverse block checksum
		u32 ead0;               // Unknown attribute
		u32 ead1;               // Unknown attribute
		u32 counter_bias;       // Bias value for the realtime clock
		s8 display_offsetH;     // Pixel offset for the VI
		u8 ntd;                 // Unknown attribute
		u8 lang;                // Language of system
		SRAMFlags flags;        // Device and operations flag

		                        // Stored configuration value from the extended SRAM area
		u8 flash_id[2][12];     // flash_id[2][12] 96bit memorycard unlock flash ID
		u32 wirelessKbd_id;     // Device ID of last connected wireless keyboard
		u16 wirelessPad_id[4];  // 16-bit device ID of last connected pad.
		u8 dvderr_code;         // last non-recoverable error from DVD interface
		u8 __padding0;          // reserved
		u8 flashID_chksum[2];   // 8-bit checksum of unlock flash ID
		u32 __padding1;         // padding
	};
};
#pragma pack(pop)
void InitSRAM();
void SetCardFlashID(u8* buffer, u8 card_index);
void FixSRAMChecksums();

extern SRAM sram_dump;
extern SRAM g_SRAM;
extern bool g_SRAM_netplay_initialized;
