// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Boot.h"
#include "../PowerPC/PowerPC.h"
#include "../HLE/HLE.h"
#include "../HW/Memmap.h"
#include "../ConfigManager.h"
#include "../IPC_HLE/WII_IPC_HLE.h"

#include "NANDContentLoader.h"
#include "FileUtil.h"
#include "Boot_DOL.h"


void SetupWiiMem()
{
    //
    // TODO: REDUDANT CODE SUX ....
    //

    Memory::Write_U32(0x5d1c9ea3, 0x00000018);		// Magic word it is a wii disc
    Memory::Write_U32(0x0D15EA5E, 0x00000020);		// Another magic word
    Memory::Write_U32(0x00000001, 0x00000024);		// Unknown
    Memory::Write_U32(0x01800000, 0x00000028);		// MEM1 size 24MB
    Memory::Write_U32(0x00000023, 0x0000002c);		// Production Board Model
    Memory::Write_U32(0x00000000, 0x00000030);		// Init
    Memory::Write_U32(0x817FEC60, 0x00000034);		// Init
    // 38, 3C should get start, size of FST through apploader
    Memory::Write_U32(0x38a00040, 0x00000060);		// Exception init
    Memory::Write_U32(0x8008f7b8, 0x000000e4);		// Thread Init
    Memory::Write_U32(0x01800000, 0x000000f0);		// "Simulated memory size" (debug mode?)
    Memory::Write_U32(0x8179b500, 0x000000f4);		// __start
    Memory::Write_U32(0x0e7be2c0, 0x000000f8);		// Bus speed
    Memory::Write_U32(0x2B73A840, 0x000000fc);		// CPU speed
    Memory::Write_U16(0x0000,     0x000030e6);		// Console type
    Memory::Write_U32(0x00000000, 0x000030c0);		// EXI
    Memory::Write_U32(0x00000000, 0x000030c4);		// EXI
    Memory::Write_U32(0x00000000, 0x000030dc);		// Time
    Memory::Write_U32(0x00000000, 0x000030d8);		// Time
    Memory::Write_U32(0x00000000, 0x000030f0);		// Apploader
    Memory::Write_U32(0x01800000, 0x00003100);		// BAT
    Memory::Write_U32(0x01800000, 0x00003104);		// BAT
    Memory::Write_U32(0x00000000, 0x0000310c);		// Init
    Memory::Write_U32(0x8179d500, 0x00003110);		// Init
    Memory::Write_U32(0x04000000, 0x00003118);		// Unknown
    Memory::Write_U32(0x04000000, 0x0000311c);		// BAT
    Memory::Write_U32(0x93400000, 0x00003120);		// BAT
    Memory::Write_U32(0x90000800, 0x00003124);		// Init - MEM2 low
    Memory::Write_U32(0x933e0000, 0x00003128);		// Init - MEM2 high
    Memory::Write_U32(0x933e0000, 0x00003130);		// IOS MEM2 low
    Memory::Write_U32(0x93400000, 0x00003134);		// IOS MEM2 high
    Memory::Write_U32(0x00000011, 0x00003138);		// Console type
    Memory::Write_U64(0x0009020400062507ULL, 0x00003140);	// IOS Version
    Memory::Write_U16(0x0113,     0x0000315e);		// Apploader
    Memory::Write_U32(0x0000FF16, 0x00003158);		// DDR ram vendor code

    Memory::Write_U8(0x80, 0x0000315c);				// OSInit
    Memory::Write_U8(0x00, 0x00000006);				// DVDInit
    Memory::Write_U8(0x00, 0x00000007);				// DVDInit
    Memory::Write_U16(0x0000, 0x000030e0);			// PADInit

    // Fake the VI Init of the BIOS 
    Memory::Write_U32(SConfig::GetInstance().m_LocalCoreStartupParameter.bNTSC ? 0 : 1, 0x000000CC);

    // Clear exception handler. Why? Don't we begin with only zeroes?
    for (int i = 0x3000; i <= 0x3038; i += 4)
    {
        Memory::Write_U32(0x00000000, 0x80000000 + i);
    }	
}

bool CBoot::IsWiiWAD(const char *filename)
{
    return DiscIO::CNANDContentLoader::IsWiiWAD(filename);
}

bool CBoot::Boot_WiiWAD(const char* _pFilename)
{        
    DiscIO::CNANDContentLoader ContentLoader(_pFilename);
    if (ContentLoader.IsValid() == false)
        return false;

    SetupWiiMem();

    // create Home directory
    char Path[260+1];
    u64 TitleID = ContentLoader.GetTitleID();
    char* pTitleID = (char*)&TitleID;
    sprintf(Path, FULL_WII_USER_DIR "title//%02x%02x%02x%02x/%02x%02x%02x%02x/data/nocopy/",
        (u8)pTitleID[7], (u8)pTitleID[6], (u8)pTitleID[5], (u8)pTitleID[4],
        (u8)pTitleID[3], (u8)pTitleID[2], (u8)pTitleID[1], (u8)pTitleID[0]);
    File::CreateDirectoryStructure(Path);

    Memory::Write_U64( ContentLoader.GetTitleID(), 0x0000318c);			// NAND Load Title ID

	// DOL
    DiscIO::SNANDContent* pContent = ContentLoader.GetContentByIndex(ContentLoader.GetBootIndex());
    if (pContent == NULL)
        return false;

    WII_IPC_HLE_Interface::SetDefaultContentFile(_pFilename);

    CDolLoader DolLoader(pContent->m_pData, pContent->m_Size);
	PC = DolLoader.GetEntryPoint() | 0x80000000;

    return true;
}

