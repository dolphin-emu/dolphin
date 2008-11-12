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

#include "VolumeCreator.h"
#include "Boot.h"

void CBoot::RunFunction(u32 _iAddr, bool _bUseDebugger)
{
	PC = _iAddr;
	LR = 0x00;

	if (_bUseDebugger)
	{
		CCPU::Break();
		while (PC != 0x00)
			CCPU::SingleStep();
	}
	else
	{
		while (PC != 0x00)
			PowerPC::SingleStep();
	}
}

// __________________________________________________________________________________________________
//
// BIOS HLE: 
// copy the apploader to 0x81200000
// execute the apploader
//
void CBoot::EmulatedBIOS(bool _bDebug)
{
	LOG(BOOT, "Faking GC BIOS...");
	UReg_MSR& m_MSR = ((UReg_MSR&)PowerPC::ppcState.msr);
	m_MSR.FP = 1;

	Memory::Clear();

	// =======================================================================================
	// Write necessary values
	// ---------------------------------------------------------------------------------------
	/*
	Here we write values to memory that the apploader does not take care of. Game iso info goes
	to 0x80000000 according to yagcd 4.2. I'm not sure what bytes 8-10 does (version and streaming),
	but I include them anyway because it seems like they are supposed to be there.
	*/
	// ---------------------------------------------------------------------------------------
	DVDInterface::DVDRead(0x00000000, 0x80000000, 10); // write boot info needed for multidisc games

	Memory::Write_U32(0x4c000064,	0x80000300);	// write default DFI Handler:		rfi
	Memory::Write_U32(0x4c000064,	0x80000800);	// write default FPU Handler:	rfi	
	Memory::Write_U32(0x4c000064,	0x80000C00);	// write default Syscall Handler:	rfi	
	//
	Memory::Write_U32(0xc2339f3d,  0x8000001C); //game disc
	Memory::Write_U32(0x0D15EA5E,  0x80000020); //funny magic word for normal boot
	Memory::Write_U32(0x01800000,	0x80000028);	// Physical Memory Size

//	Memory::Write_U32(0x00000003,	0x8000002C);	// Console type - retail
	Memory::Write_U32(0x10000006,	0x8000002C);	// DevKit

	Memory::Write_U32(((1 & 0x3f) << 26) | 2, 0x81300000);		// HLE OSReport for Apploader
	// =======================================================================================


	// =======================================================================================
	// Load Apploader to Memory - The apploader is hardcoded to begin at byte 9 280 on the disc,
	// but it seems like the size can be variable. Compare with yagcd chap 13.
	// ---------------------------------------------------------------------------------------
	PowerPC::ppcState.gpr[1] = 0x816ffff0;			// StackPointer
	u32 iAppLoaderOffset = 0x2440; // 0x1c40 (what is 0x1c40?)
	// ---------------------------------------------------------------------------------------
	u32 iAppLoaderEntry = VolumeHandler::Read32(iAppLoaderOffset + 0x10);
	u32 iAppLoaderSize  = VolumeHandler::Read32(iAppLoaderOffset + 0x14);
	if ((iAppLoaderEntry == (u32)-1) || (iAppLoaderSize == (u32)-1))
		return;
	VolumeHandler::ReadToPtr(Memory::GetPointer(0x81200000), iAppLoaderOffset + 0x20, iAppLoaderSize);
	// =======================================================================================


	//call iAppLoaderEntry
	LOG(MASTER_LOG, "Call iAppLoaderEntry");

	u32 iAppLoaderFuncAddr = 0x80003100;
	PowerPC::ppcState.gpr[3] = iAppLoaderFuncAddr + 0;
	PowerPC::ppcState.gpr[4] = iAppLoaderFuncAddr + 4;
	PowerPC::ppcState.gpr[5] = iAppLoaderFuncAddr + 8;	
	RunFunction(iAppLoaderEntry, _bDebug);
	u32 iAppLoaderInit = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+0);
	u32 iAppLoaderMain = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+4);
	u32 iAppLoaderClose = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+8);
	
	// iAppLoaderInit
	LOG(MASTER_LOG, "Call iAppLoaderInit");
	PowerPC::ppcState.gpr[3] = 0x81300000; 
	RunFunction(iAppLoaderInit, _bDebug);

	
	// =======================================================================================
	/*
	iAppLoaderMain - Here we load the apploader, the DOL (the exe) and the FST (filesystem).
	To give you an idea about where the stuff is located on the disc take a look at yagcd chap 13.
	*/
	// ---------------------------------------------------------------------------------------	
	LOG(MASTER_LOG, "Call iAppLoaderMain");
	do
	{
		PowerPC::ppcState.gpr[3] = 0x81300004;
		PowerPC::ppcState.gpr[4] = 0x81300008;
		PowerPC::ppcState.gpr[5] = 0x8130000c;

		RunFunction(iAppLoaderMain, _bDebug);

		u32 iRamAddress	= Memory::ReadUnchecked_U32(0x81300004);
		u32 iLength		= Memory::ReadUnchecked_U32(0x81300008);
		u32 iDVDOffset	= Memory::ReadUnchecked_U32(0x8130000c);
		
		LOGV(MASTER_LOG, 2, "DVDRead: offset: %08x   memOffset: %08x   length: %i", iDVDOffset, iRamAddress, iLength);
		DVDInterface::DVDRead(iDVDOffset, iRamAddress, iLength);

	} while(PowerPC::ppcState.gpr[3] != 0x00);
	// =======================================================================================


	// iAppLoaderClose
	LOG(MASTER_LOG, "call iAppLoaderClose");
	RunFunction(iAppLoaderClose, _bDebug);

	// Load patches and run startup patches
	std::string gameID = VolumeHandler::GetVolume()->GetUniqueID();
	PatchEngine_LoadPatches(gameID.c_str());
	PatchEngine_ApplyLoadPatches();
	PowerPC::ppcState.DebugCount = 0;
	// return
	PC = PowerPC::ppcState.gpr[3];

    // --- preinit some stuff from bios ---

    // Bus Clock Speed
    Memory::Write_U32(0x09a7ec80, 0x800000F8);
    Memory::Write_U32(0x1cf7c580, 0x800000FC);

    // fake the VI Init of the BIOS 
    Memory::Write_U32(Core::g_CoreStartupParameter.bNTSC ? 0 : 1, 0x800000CC);

    // preset time
    Memory::Write_U32(CEXIIPL::GetGCTime(), 0x800030D8);
}


// __________________________________________________________________________________________________
//
// BIOS HLE: 
// copy the apploader to 0x81200000
// execute the apploader
//
bool CBoot::EmulatedBIOS_Wii(bool _bDebug)
{	
	LOG(BOOT, "Faking Wii BIOS...");

    FILE* pDump = fopen(FULL_WII_SYS_DIR "dump_0x0000_0x4000.bin", "rb");
    if (pDump != NULL)
    {
        LOG(MASTER_LOG, "Init from memory dump.");	

        fread(Memory::GetMainRAMPtr(), 1, 16384, pDump);
        fclose(pDump);
        pDump = NULL;
    }
    else
    {
	    // load settings.txt
	    {
		    std::string filename(WII_EUR_SETTING_FILE);
		    if (VolumeHandler::IsValid())
		    {
			    switch(VolumeHandler::GetVolume()->GetCountry())
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
				    PanicAlert("Unknown country. Wii boot process will be switched to European settings.");
				    filename = WII_EUR_SETTING_FILE;
				    break;
			    }
		    }

		    FILE* pTmp = fopen(filename.c_str(), "rb");
		    if (!pTmp)
		    {
			    LOG(MASTER_LOG, "Cant find setting file");	
			    return false;
		    }

		    fread(Memory::GetPointer(0x3800), 256, 1, pTmp);
		    fclose(pTmp);
	    }

	    // int global vars
	    {
		    // updated with info from wiibrew.org
		    Memory::Write_U32(0x5d1c9ea3, 0x00000018);		// magic word it is a wii disc
		    Memory::Write_U32(0x0D15EA5E, 0x00000020);		
		    Memory::Write_U32(0x00000001, 0x00000024);		
		    Memory::Write_U32(0x01800000, 0x00000028);		
		    Memory::Write_U32(0x00000023, 0x0000002c);
		    Memory::Write_U32(0x00000000, 0x00000030);		// Init
		    Memory::Write_U32(0x817FEC60, 0x00000034);		// Init
		    // 38, 3C should get start, size of FST through apploader
		    Memory::Write_U32(0x38a00040, 0x00000060);		// Exception init
		    Memory::Write_U32(0x8008f7b8, 0x000000e4);		// Thread Init
		    Memory::Write_U32(0x01800000, 0x000000f0);		// "simulated memory size" (debug mode?)
		    Memory::Write_U32(0x8179b500, 0x000000f4);		// __start
		    Memory::Write_U32(0x0e7be2c0, 0x000000f8);		// Bus speed
		    Memory::Write_U32(0x2B73A840, 0x000000fc);		// CPU speed
		    Memory::Write_U16(0x0000,     0x000030e6);		// Console type
		    Memory::Write_U32(0x00000000, 0x000030c0);		// EXI
		    Memory::Write_U32(0x00000000, 0x000030c4);		// EXI
		    Memory::Write_U32(0x00000000, 0x000030dc);		// Time
		    Memory::Write_U32(0x00000000, 0x000030d8);		// Time
		    Memory::Write_U32(0x00000000, 0x000030f0);		// apploader
		    Memory::Write_U32(0x01800000, 0x00003100);		// BAT
		    Memory::Write_U32(0x01800000, 0x00003104);		// BAT
		    Memory::Write_U32(0x00000000, 0x0000310c);		// Init
		    Memory::Write_U32(0x8179d500, 0x00003110);		// Init
		    Memory::Write_U32(0x04000000, 0x00003118);
		    Memory::Write_U32(0x04000000, 0x0000311c);		// BAT
		    Memory::Write_U32(0x93400000, 0x00003120);		// BAT
		    Memory::Write_U32(0x90000800, 0x00003124);		// Init - MEM2 low
		    Memory::Write_U32(0x933e0000, 0x00003128);		// Init - MEM2 high
		    Memory::Write_U32(0x933e0000, 0x00003130);		// IOS MEM2 low
		    Memory::Write_U32(0x93400000, 0x00003134);		// IOS MEM2 high
		    Memory::Write_U32(0x00000011, 0x00003138);		// Console type
		    Memory::Write_U16(0x0113,     0x0000315e);		// apploader
		    Memory::Write_U32(0x0000FF16, 0x00003158);		// DDR ram vendor code
		    Memory::Write_U8(0x80, 0x0000315c);				// OSInit
		    Memory::Write_U8(0x00, 0x00000006);				// DVDInit
		    Memory::Write_U8(0x00, 0x00000007);				// DVDInit
		    Memory::Write_U16(0x0000, 0x000030e0);			// PADInit

			// fake the VI Init of the BIOS 
			Memory::Write_U32(Core::g_CoreStartupParameter.bNTSC ? 0 : 1, 0x000000CC);

		    // clear exception handler
		    for (int i = 0x3000; i <= 0x3038; i += 4)
		    {
			    Memory::Write_U32(0x00000000, 0x80000000 + i);
		    }	

		    // app
		    VolumeHandler::ReadToPtr(Memory::GetPointer(0x3180), 0, 4);
		    Memory::Write_U8(0x80, 0x00003184);
	    }
    }

	// apploader
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
			LOG(BOOT, "Invalid apploader. Probably your image is corrupted.");
			return false;
		}
		VolumeHandler::ReadToPtr(Memory::GetPointer(0x81200000), iAppLoaderOffset + 0x20, iAppLoaderSize);

		//call iAppLoaderEntry
		LOG(BOOT, "Call iAppLoaderEntry");

		u32 iAppLoaderFuncAddr = 0x80004000;
		PowerPC::ppcState.gpr[3] = iAppLoaderFuncAddr + 0;
		PowerPC::ppcState.gpr[4] = iAppLoaderFuncAddr + 4;
		PowerPC::ppcState.gpr[5] = iAppLoaderFuncAddr + 8;	
		RunFunction(iAppLoaderEntry, _bDebug);
		u32 iAppLoaderInit = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+0);
		u32 iAppLoaderMain = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+4);
		u32 iAppLoaderClose = Memory::ReadUnchecked_U32(iAppLoaderFuncAddr+8);

		// iAppLoaderInit
		LOG(BOOT, "Call iAppLoaderInit");
		PowerPC::ppcState.gpr[3] = 0x81300000; 
		RunFunction(iAppLoaderInit, _bDebug);

		// iAppLoaderMain
		LOG(BOOT, "Call iAppLoaderMain");
		do
		{
			PowerPC::ppcState.gpr[3] = 0x81300004;
			PowerPC::ppcState.gpr[4] = 0x81300008;
			PowerPC::ppcState.gpr[5] = 0x8130000c;

			RunFunction(iAppLoaderMain, _bDebug);

			u32 iRamAddress	= Memory::ReadUnchecked_U32(0x81300004);
			u32 iLength		= Memory::ReadUnchecked_U32(0x81300008);
			u32 iDVDOffset	= Memory::ReadUnchecked_U32(0x8130000c) << 2;

			LOGV(BOOT, 1, "DVDRead: offset: %08x   memOffse: %08x   length: %i", iDVDOffset, iRamAddress, iLength);
			DVDInterface::DVDRead(iDVDOffset, iRamAddress, iLength);
		} while(PowerPC::ppcState.gpr[3] != 0x00);

		// iAppLoaderClose
		LOG(BOOT, "call iAppLoaderClose");
		RunFunction(iAppLoaderClose, _bDebug);

		// Load patches and run startup patches
		std::string gameID = VolumeHandler::GetVolume()->GetUniqueID();
		PatchEngine_LoadPatches(gameID.c_str());

		// return
		PC = PowerPC::ppcState.gpr[3];
	}

	PowerPC::ppcState.DebugCount = 0;

	return true;
}

