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

#include "Common.h"

#include "../PowerPC/PowerPC.h"
#include "../Core.h"
#include "../HW/EXI_DeviceIPL.h"
#include "../HW/Memmap.h"
#include "../HW/DVDInterface.h"
#include "../HW/CPU.h"

#include "../Host.h"
#include "../VolumeHandler.h"
#include "../PatchEngine.h"
#include "../MemTools.h"

#include "../ConfigManager.h"
#include "VolumeCreator.h"
#include "Boot.h"

void CBoot::RunFunction(u32 _iAddr)
{
	PC = _iAddr;
	LR = 0x00;

	while (PC != 0x00)
		PowerPC::SingleStep();
}

// __________________________________________________________________________________________________
//
// GameCube BIOS HLE: 
// copy the apploader to 0x81200000
// execute the apploader, function by function, using the above utility.
void CBoot::EmulatedBIOS(bool _bDebug)
{
	INFO_LOG(BOOT, "Faking GC BIOS...");
	UReg_MSR& m_MSR = ((UReg_MSR&)PowerPC::ppcState.msr);
	m_MSR.FP = 1;

	Memory::Clear();

	// =======================================================================================
	// Write necessary values
	//
	// Here we write values to memory that the apploader does not take care of. Game iso info goes
	// to 0x80000000 according to yagcd 4.2. I'm not sure what bytes 8-10 does (version and
	//  streaming), but I include them anyway because it seems like they are supposed to be there. 
	//
	DVDInterface::DVDRead(0x00000000, 0x80000000, 10); // write boot info needed for multidisc games

	Memory::Write_U32(0x4c000064,	0x80000300);	// write default DFI Handler:	    rfi
	Memory::Write_U32(0x4c000064,	0x80000800);	// write default FPU Handler:	    rfi	
	Memory::Write_U32(0x4c000064,	0x80000C00);	// write default Syscall Handler:   rfi	
	Memory::Write_U32(0xc2339f3d,   0x8000001C);    // game disc
	Memory::Write_U32(0x0D15EA5E,   0x80000020);    // funny magic word for normal boot
	Memory::Write_U32(0x01800000,	0x80000028);	// Physical Memory Size

	// On any of the production boards, ikaruga fails to read the memcard the first time. It succeeds on the second time though.
	// And (only sometimes?) with 0x00000003, the loading picture in the bottom right will become corrupt and
	// the emu will slow to 7mhz...I don't think it ever actually progresses
	Memory::Write_U32(0x10000006,	0x8000002C);	// Console type - DevKit  (retail ID == 0x00000003) see yagcd 4.2.1.1.2

	Memory::Write_U32(((1 & 0x3f) << 26) | 2, 0x81300000);		// HLE OSReport for Apploader

	// Load Apploader to Memory - The apploader is hardcoded to begin at byte 9 280 on the disc,
	// but it seems like the size can be variable. Compare with yagcd chap 13.
	// 
	PowerPC::ppcState.gpr[1] = 0x816ffff0;			// StackPointer
	u32 iAppLoaderOffset = 0x2440;                  // 0x1c40 (what is 0x1c40?)
	u32 iAppLoaderEntry = VolumeHandler::Read32(iAppLoaderOffset + 0x10);
	u32 iAppLoaderSize  = VolumeHandler::Read32(iAppLoaderOffset + 0x14);
	if ((iAppLoaderEntry == (u32)-1) || (iAppLoaderSize == (u32)-1))
		return;
	VolumeHandler::ReadToPtr(Memory::GetPointer(0x81200000), iAppLoaderOffset + 0x20, iAppLoaderSize);

	// Call iAppLoaderEntry.
	DEBUG_LOG(MASTER_LOG, "Call iAppLoaderEntry");

	u32 iAppLoaderFuncAddr = 0x80003100;
	PowerPC::ppcState.gpr[3] = iAppLoaderFuncAddr + 0;
	PowerPC::ppcState.gpr[4] = iAppLoaderFuncAddr + 4;
	PowerPC::ppcState.gpr[5] = iAppLoaderFuncAddr + 8;	
	RunFunction(iAppLoaderEntry);
	u32 iAppLoaderInit = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr + 0);
	u32 iAppLoaderMain = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr + 4);
	u32 iAppLoaderClose = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr + 8);
	
	// iAppLoaderInit
	DEBUG_LOG(MASTER_LOG, "Call iAppLoaderInit");
	PowerPC::ppcState.gpr[3] = 0x81300000; 
	RunFunction(iAppLoaderInit);
	
	// iAppLoaderMain - Here we load the apploader, the DOL (the exe) and the FST (filesystem).
	// To give you an idea about where the stuff is located on the disc take a look at yagcd
	// ch 13.
	DEBUG_LOG(MASTER_LOG, "Call iAppLoaderMain");
	do
	{
		PowerPC::ppcState.gpr[3] = 0x81300004;
		PowerPC::ppcState.gpr[4] = 0x81300008;
		PowerPC::ppcState.gpr[5] = 0x8130000c;

		RunFunction(iAppLoaderMain);

		u32 iRamAddress	= Memory::ReadUnchecked_U32(0x81300004);
		u32 iLength		= Memory::ReadUnchecked_U32(0x81300008);
		u32 iDVDOffset	= Memory::ReadUnchecked_U32(0x8130000c);
		
		INFO_LOG(MASTER_LOG, "DVDRead: offset: %08x   memOffset: %08x   length: %i", iDVDOffset, iRamAddress, iLength);
		DVDInterface::DVDRead(iDVDOffset, iRamAddress, iLength);

	} while(PowerPC::ppcState.gpr[3] != 0x00);

	// iAppLoaderClose
	DEBUG_LOG(MASTER_LOG, "call iAppLoaderClose");
	RunFunction(iAppLoaderClose);

	// Load patches
	std::string gameID = VolumeHandler::GetVolume()->GetUniqueID();
	PatchEngine::LoadPatches(gameID.c_str());
	PowerPC::ppcState.DebugCount = 0;
	// return
	PC = PowerPC::ppcState.gpr[3];

    // --- preinit some stuff from bios ---

    // Bus Clock Speed
    Memory::Write_U32(0x09a7ec80, 0x800000F8);
    // CPU Clock Speed
    Memory::Write_U32(0x1cf7c580, 0x800000FC);

    // fake the VI Init of the BIOS 
    Memory::Write_U32(SConfig::GetInstance().m_LocalCoreStartupParameter.bNTSC 
		      ? 0 : 1, 0x800000CC);

    // preset time base ticks
    Memory::Write_U64( (u64)CEXIIPL::GetGCTime() * (u64)40500000, 0x800030D8);
}



bool CBoot::SetupWiiMemory(unsigned int _CountryCode)
{
    INFO_LOG(BOOT, "Setup Wii Memory...");

    // Write the 256 byte setting.txt to memory. This may not be needed as
    // most or all games read the setting.txt file from \title\00000001\00000002\
    // data\setting.txt directly after the read the SYSCONF file. The games also
    // read it to 0x3800, what is a little strange however is that it only reads
    // the first 100 bytes of it.
    std::string filename(WII_EUR_SETTING_FILE);
    switch((DiscIO::IVolume::ECountry)_CountryCode)
    {
    case DiscIO::IVolume::COUNTRY_JAP:
        filename = WII_JAP_SETTING_FILE;
        break;

    case DiscIO::IVolume::COUNTRY_USA:
        filename = WII_USA_SETTING_FILE;
        break;

    case DiscIO::IVolume::COUNTRY_EUROPE:
        filename = WII_EUR_SETTING_FILE;
        break;

    default:
        PanicAlert("SetupWiiMem: Unknown country. Wii boot process will be switched to European settings.");
        filename = WII_EUR_SETTING_FILE;
        break;
    }

    FILE* pTmp = fopen(filename.c_str(), "rb");
    if (!pTmp)
    {
        PanicAlert("SetupWiiMem: Cant find setting file");	
        return false;
    }

    fread(Memory::GetPointer(0x3800), 256, 1, pTmp);
    fclose(pTmp);


    /* Set hardcoded global variables to Wii memory. These are partly collected from
    Wiibrew. These values are needed for the games to function correctly. A few
    values in this region will also be placed here by the game as it boots.
    They are:

    // Strange values that I don't know the meaning of, all games write these
    0x00 to 0x18:	0x029f0010
    0x029f0033
    0x029f0034
    0x029f0035
    0x029f0036
    0x029f0037
    0x029f0038
    0x029f0039 // Replaces the previous 0x5d1c9ea3 magic word

    0x80000038	Start of FST
    0x8000003c	Size of FST Size
    0x80000060	Copyright code */	

    DVDInterface::DVDRead(0x00000000, 0x00000000, 6); // Game Code
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
    Memory::Write_U32(0x93ae0000, 0x00003128);		// Init - MEM2 high
    Memory::Write_U32(0x93ae0000, 0x00003130);		// IOS MEM2 low       
    Memory::Write_U32(0x93b00000, 0x00003134);		// IOS MEM2 high
    Memory::Write_U32(0x00000011, 0x00003138);		// Console type
    Memory::Write_U64(0x0009020400062507ULL, 0x00003140);	// IOS Version
    Memory::Write_U16(0x0113,     0x0000315e);		// Apploader
    Memory::Write_U32(0x0000FF16, 0x00003158);		// DDR ram vendor code

    Memory::Write_U8(0x80, 0x0000315c);				// OSInit
    Memory::Write_U8(0x00, 0x00000006);				// DVDInit
    Memory::Write_U8(0x00, 0x00000007);				// DVDInit
    Memory::Write_U16(0x0000, 0x000030e0);			// PADInit
    Memory::Write_U32(0x80000000, 0x00003184);             // GameID Address

    // Fake the VI Init of the BIOS 
    Memory::Write_U32(SConfig::GetInstance().m_LocalCoreStartupParameter.bNTSC ? 0 : 1, 0x000000CC);

    // Clear exception handler. Why? Don't we begin with only zeroes?
    for (int i = 0x3000; i <= 0x3038; i += 4)
    {
        Memory::Write_U32(0x00000000, 0x80000000 + i);
    }	

    return true;
}

// __________________________________________________________________________________________________
//
// Wii BIOS HLE: 
// copy the apploader to 0x81200000
// execute the apploader
//
bool CBoot::EmulatedBIOS_Wii(bool _bDebug)
{	
	INFO_LOG(BOOT, "Faking Wii BIOS...");

    // setup wii memory
    DiscIO::IVolume::ECountry CountryCode = DiscIO::IVolume::COUNTRY_UNKNOWN;
    if (VolumeHandler::IsValid())
        CountryCode = VolumeHandler::GetVolume()->GetCountry();
    if (SetupWiiMemory(CountryCode) == false)
        return false;

    // This is some kind of consistency check that is compared to the 0x00
    // values as the game boots. This location keep the 4 byte ID for as long
    // as the game is running. The 6 byte ID at 0x00 is overwritten sometime
    // after this check during booting. 
    VolumeHandler::ReadToPtr(Memory::GetPointer(0x3180), 0, 4);

	// Execute the apploader
    if (VolumeHandler::IsValid() && VolumeHandler::IsWii())	
	{
		UReg_MSR& m_MSR = ((UReg_MSR&)PowerPC::ppcState.msr);
		m_MSR.FP = 1;

		//TODO: Game iso info to 0x80000000 according to yagcd - or does apploader do this?

		Memory::Write_U32(0x4c000064,	0x80000300);	// write default DFI Handler:		rfi
		Memory::Write_U32(0x4c000064,	0x80000800);	// write default FPU Handler:	    rfi	
		Memory::Write_U32(0x4c000064,	0x80000C00);	// write default Syscall Handler:	rfi	

		Memory::Write_U32(((1 & 0x3f) << 26) | 2, 0x81300000);		// HLE OSReport for Apploader

		PowerPC::ppcState.gpr[1] = 0x816ffff0;			// StackPointer

		u32 iAppLoaderOffset = 0x2440; // 0x1c40;

		// Load Apploader to Memory
		u32 iAppLoaderEntry = VolumeHandler::Read32(iAppLoaderOffset + 0x10);
		u32 iAppLoaderSize = VolumeHandler::Read32(iAppLoaderOffset + 0x14);
		if ((iAppLoaderEntry == (u32)-1) || (iAppLoaderSize == (u32)-1)) 
		{
			ERROR_LOG(BOOT, "Invalid apploader. Probably your image is corrupted.");
			return false;
		}
		VolumeHandler::ReadToPtr(Memory::GetPointer(0x81200000), iAppLoaderOffset + 0x20, iAppLoaderSize);

		//call iAppLoaderEntry
		DEBUG_LOG(BOOT, "Call iAppLoaderEntry");

		u32 iAppLoaderFuncAddr = 0x80004000;
		PowerPC::ppcState.gpr[3] = iAppLoaderFuncAddr + 0;
		PowerPC::ppcState.gpr[4] = iAppLoaderFuncAddr + 4;
		PowerPC::ppcState.gpr[5] = iAppLoaderFuncAddr + 8;	
		RunFunction(iAppLoaderEntry);
		u32 iAppLoaderInit = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+0);
		u32 iAppLoaderMain = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+4);
		u32 iAppLoaderClose = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+8);

		// iAppLoaderInit
		DEBUG_LOG(BOOT, "Run iAppLoaderInit");
		PowerPC::ppcState.gpr[3] = 0x81300000; 
		RunFunction(iAppLoaderInit);

		// Let the apploader load the exe to memory. At this point I get an unknwon IPC command
		// (command zero) when I load Wii Sports or other games a second time. I don't notice
		// any side effects however. It's a little disconcerning however that Start after Stop
		// behaves differently than the first Start after starting Dolphin. It means something
		// was not reset correctly. 
		DEBUG_LOG(BOOT, "Run iAppLoaderMain");
		do
		{
			PowerPC::ppcState.gpr[3] = 0x81300004;
			PowerPC::ppcState.gpr[4] = 0x81300008;
			PowerPC::ppcState.gpr[5] = 0x8130000c;

			RunFunction(iAppLoaderMain);

			u32 iRamAddress	= Memory::ReadUnchecked_U32(0x81300004);
			u32 iLength		= Memory::ReadUnchecked_U32(0x81300008);
			u32 iDVDOffset	= Memory::ReadUnchecked_U32(0x8130000c) << 2;

			INFO_LOG(BOOT, "DVDRead: offset: %08x   memOffse: %08x   length: %i", iDVDOffset, iRamAddress, iLength);
			DVDInterface::DVDRead(iDVDOffset, iRamAddress, iLength);
		} while(PowerPC::ppcState.gpr[3] != 0x00);

		// iAppLoaderClose
		DEBUG_LOG(BOOT, "Run iAppLoaderClose");
		RunFunction(iAppLoaderClose);

		// Load patches and run startup patches
		std::string gameID = VolumeHandler::GetVolume()->GetUniqueID();
		PatchEngine::LoadPatches(gameID.c_str());

		// return
		PC = PowerPC::ppcState.gpr[3];
	}

	PowerPC::ppcState.DebugCount = 0;

	return true;
}

