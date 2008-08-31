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
#include "StringUtil.h"

#include "../HLE/HLE.h"

#include "../PowerPC/PowerPC.h"
#include "../PowerPC/PPCAnalyst.h"
#include "../Core.h"
#include "../HW/HW.h"
#include "../HW/EXI_DeviceIPL.h"
#include "../HW/Memmap.h"
#include "../HW/PeripheralInterface.h"
#include "../HW/DVDInterface.h"
#include "../HW/VideoInterface.h"
#include "../HW/CPU.h"

#include "../Debugger/Debugger_SymbolMap.h"
#include "../Debugger/Debugger_BreakPoints.h"

#include "Boot_DOL.h"
#include "Boot.h"
#include "../Host.h"
#include "../VolumeHandler.h"
#include "../PatchEngine.h"
#include "../PowerPC/SignatureDB.h"
#include "../PowerPC/SymbolDB.h"

#include "../MemTools.h"
#include "MappedFile.h"

#include "VolumeCreator.h"

bool CBoot::Boot_BIN(const std::string& _rFilename)
{
	Common::IMappedFile* pFile = Common::IMappedFile::CreateMappedFile();

	if (pFile->Open(_rFilename.c_str()))
	{
		u8* pData = pFile->Lock(0, pFile->GetSize());
		Memory::WriteBigEData(pData, 0x80000000, (u32)pFile->GetSize());
		pFile->Unlock(pData);
		pFile->Close();
	}

	delete pFile;
    return true;
}

void WrapRunFunction(void*)
{
	while (PC != 0x00)
	{
		CCPU::SingleStep();
	}
}

void WrapRunFunction2(void*)
{
	while (PC != 0x00)
	{
		PowerPC::SingleStep();
	}
}

void CBoot::RunFunction(u32 _iAddr, bool _bUseDebugger)
{
	PC = _iAddr;
	LR = 0x00;

	if (_bUseDebugger)
	{
		CCPU::Break();
		//EMM::Wrap(WrapRunFunction,0);
		WrapRunFunction(0);
	}
	else
	{
		//EMM::Wrap(WrapRunFunction2,0);
		WrapRunFunction2(0);
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

	//TODO: Game iso info to 0x80000000 according to yagcd - or does apploader do this?

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

	PowerPC::ppcState.gpr[1] = 0x816ffff0;			// StackPointer

	u32 iAppLoaderOffset = 0x2440; // 0x1c40;

	// Load Apploader to Memory
	u32 iAppLoaderEntry = VolumeHandler::Read32(iAppLoaderOffset + 0x10);
	u32 iAppLoaderSize  = VolumeHandler::Read32(iAppLoaderOffset + 0x14);
	if ((iAppLoaderEntry == (u32)-1) || (iAppLoaderSize == (u32)-1))
		return;
	VolumeHandler::ReadToPtr(Memory::GetPointer(0x81200000), iAppLoaderOffset + 0x20, iAppLoaderSize);
	
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
	
	// iAppLoaderMain
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
		
		LOG(MASTER_LOG, "DVDRead: offset: %08x   memOffset: %08x   length: %i", iDVDOffset, iRamAddress, iLength);
		DVDInterface::DVDRead(iDVDOffset, iRamAddress, iLength);

	} while(PowerPC::ppcState.gpr[3] != 0x00);

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

	// load settings.txt
	{
		std::string filename("WII/setting-eur.txt");
		if (VolumeHandler::IsValid())
		{
			switch(VolumeHandler::GetVolume()->GetCountry())
			{
			case DiscIO::IVolume::COUNTRY_JAP:
				filename = "WII/setting-jpn.txt";
				break;

			case DiscIO::IVolume::COUNTRY_USA:
				filename = "WII/setting-usa.txt";
				break;

			case DiscIO::IVolume::COUNTRY_EUROPE:
				filename = "WII/setting-eur.txt";
				break;

			default:
				PanicAlert("Unknown country. Wii boot process will be switched to European settings.");
				filename = "WII/setting-eur.txt";
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
		Memory::Write_U32(0x00000005, 0x000000cc);		// VIInit
		Memory::Write_U16(0x0000, 0x000030e0);			// PADInit

		// clear exception handler
		for (int i = 0x3000; i <= 0x3038; i += 4)
		{
			Memory::Write_U32(0x00000000, 0x80000000 + i);
		}	

		// app
		VolumeHandler::ReadToPtr(Memory::GetPointer(0x3180), 0, 4);
		Memory::Write_U8(0x80, 0x00003184);
	}

	// apploader
	if (VolumeHandler::IsValid())	
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

			LOG(BOOT, "DVDRead: offset: %08x   memOffse: %08x   length: %i", iDVDOffset, iRamAddress, iLength);
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


void CBoot::UpdateDebugger_MapLoaded(const char *_gameID)
{
	Host_NotifyMapLoaded();
	Host_UpdateMemoryView();
}

std::string CBoot::GenerateMapFilename()
{
	/*
	std::string strDriveDirectory, strFilename;
	SplitPath(booted_file, &strDriveDirectory, &strFilename, NULL);
	
	std::string strFullfilename(strFilename + _T(".map"));
	std::string strMapFilename;
	BuildCompleteFilename(strMapFilename, strDriveDirectory, strFullfilename);
	*/
	return "Maps/" + Core::GetStartupParameter().GetUniqueID() + ".map";
}

bool CBoot::LoadMapFromFilename(const std::string &_rFilename, const char *_gameID)
{
	if (_rFilename.size() == 0)
		return false;

	std::string strMapFilename = GenerateMapFilename();

	bool success = false;
    if (!g_symbolDB.LoadMap(strMapFilename.c_str()))
	{
		if (_gameID != NULL)
		{
			BuildCompleteFilename(strMapFilename, _T("maps"), std::string(_gameID) + _T(".map"));
            success = g_symbolDB.LoadMap(strMapFilename.c_str());
		}
	}
	else
	{
		success = true;
	}

	if (success)
		UpdateDebugger_MapLoaded();

	return success;
}

bool CBoot::Load_BIOS(const std::string& _rBiosFilename)
{
    bool bResult = false;
    Common::IMappedFile* pFile = Common::IMappedFile::CreateMappedFile();
    if (pFile->Open(_rBiosFilename.c_str()))
    {
        if (pFile->GetSize() >= 1024*1024*2)
        {
            u32 CopySize = (u32)pFile->GetSize() - 0x820;
            u8* pData = pFile->Lock(0x820, CopySize);
            Memory::WriteBigEData(pData, 0x81300000, CopySize);
            pFile->Unlock(pData);
            pFile->Close();

            PC = 0x81300000;

            bResult = true;
        }
    }
    delete pFile;
    return bResult;
}

bool CBoot::BootUp(const SCoreStartupParameter& _StartupPara)
{
    const bool bDebugIsoBootup = false;

	g_symbolDB.Clear();
    VideoInterface::PreInit(_StartupPara.bNTSC);
    switch(_StartupPara.m_BootType)
    {
    // GCM
    // ===================================================================================
    case SCoreStartupParameter::BOOT_ISO:
        {
            DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(_StartupPara.m_strFilename);
            if (pVolume == NULL)
                break;

            bool isoWii = DiscIO::IsVolumeWiiDisc(*pVolume);           
			if (isoWii != Core::GetStartupParameter().bWii)
			{
				PanicAlert("Warning - starting ISO in wrong console mode!");
			}

            char gameID[7];
            memcpy(gameID, pVolume->GetUniqueID().c_str(), 6);
			gameID[6] = 0;

            // setup the map from ISOFile ID
            VolumeHandler::SetVolumeName(_StartupPara.m_strFilename);

            DVDInterface::SetDiscInside(true);

            if (_StartupPara.bHLEBios)
            {
                if (!VolumeHandler::IsWii())
		            EmulatedBIOS(bDebugIsoBootup);
				else
				{
					Core::g_CoreStartupParameter.bWii = true;
					EmulatedBIOS_Wii(bDebugIsoBootup);
				}
            } 
            else
            {
                if (!Load_BIOS(_StartupPara.m_strBios))
                {
                    // fails to load a BIOS so HLE it
                    if (!VolumeHandler::IsWii())
						EmulatedBIOS(bDebugIsoBootup);
					else
					{
						Core::g_CoreStartupParameter.bWii = true;
						EmulatedBIOS_Wii(bDebugIsoBootup);
					}
                }
            }
            if (LoadMapFromFilename(_StartupPara.m_strFilename, gameID))
                HLE::PatchFunctions();
        }
        break;

    // DOL
    // ===================================================================================
    case SCoreStartupParameter::BOOT_DOL:
        {
            CDolLoader dolLoader(_StartupPara.m_strFilename.c_str());
            PC = dolLoader.GetEntryPoint();
#ifdef _DEBUG
            if (LoadMapFromFilename(_StartupPara.m_strFilename))
				HLE::PatchFunctions();
#endif
        }
        break;

    // ELF
    // ===================================================================================
    case SCoreStartupParameter::BOOT_ELF:
        {
			bool elfWii = IsElfWii(_StartupPara.m_strFilename.c_str());
			if (elfWii != Core::GetStartupParameter().bWii)
			{
				PanicAlert("Warning - starting ELF in wrong console mode!");
			}

			VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM);			
			if (elfWii)
			{                              
                if (VolumeHandler::IsWii() && (!_StartupPara.m_strDefaultGCM.empty()))
                    VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM.c_str());

				EmulatedBIOS_Wii(false);
			}
			else
			{
				if (!VolumeHandler::IsWii() && !_StartupPara.m_strDefaultGCM.empty())
				{
					VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM.c_str());
					EmulatedBIOS(false);
				}
			}
			
            Boot_ELF(_StartupPara.m_strFilename.c_str()); 
            UpdateDebugger_MapLoaded();
			CBreakPoints::AddAutoBreakpoints();
        }
        break;

    // BIN
    // ===================================================================================
    case SCoreStartupParameter::BOOT_BIN:
        {
			if (!_StartupPara.m_strDefaultGCM.empty())
			{
				VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM.c_str());
				EmulatedBIOS(false);
			}

            Boot_BIN(_StartupPara.m_strFilename); 
            if (LoadMapFromFilename(_StartupPara.m_strFilename))
				HLE::PatchFunctions();
        }
        break;

    // BIOS
    // ===================================================================================
    case SCoreStartupParameter::BOOT_BIOS:
        {
            DVDInterface::SetDiscInside(false);
            if (Load_BIOS(_StartupPara.m_strBios))
            {
                if (LoadMapFromFilename(_StartupPara.m_strFilename))
					HLE::PatchFunctions();
            }
            else
            {
                return false;
            }
        }
        break;

    default:
        {
            PanicAlert("Tried to load an unknown file type.");
            return false;
        }
    }
	Host_UpdateLogDisplay();
	return true;
}
