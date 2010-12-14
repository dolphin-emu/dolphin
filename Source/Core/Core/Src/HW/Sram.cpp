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

#include "Sram.h"
#include "../ConfigManager.h"

void initSRAM()
{
	FILE *file = fopen(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strSRAM.c_str(), "rb");
    if (file != NULL)
    {
        if (fread(&g_SRAM, 1, 64, file) < 64) {
	    ERROR_LOG(EXPANSIONINTERFACE,  "EXI IPL-DEV: Could not read all of SRAM");
	    g_SRAM = sram_dump;
	}
        fclose(file);
    }
    else
    {
	    g_SRAM = sram_dump;
    }
}

void SetCardFlashID(u8* buffer, u8 card_index)
{
	u64 rand = Common::swap64( *(u64*)&(buffer[12]));
	u8 csum=0;
	for(int i = 0; i < 12; i++)
	{
		rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
		csum += g_SRAM.flash_id[card_index][i] = buffer[i] - ((u8)rand&0xff);
		rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);	
		rand &= (u64)0x0000000000007fffULL;
	}
	g_SRAM.flashID_chksum[card_index] = csum^0xFF;
}
