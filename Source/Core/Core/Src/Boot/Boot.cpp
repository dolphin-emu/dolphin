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


#include "Common.h" // Common
#include "StringUtil.h"
#include "FileUtil.h"
#include "MappedFile.h"

#include "../HLE/HLE.h" // Core
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

#include "../Debugger/Debugger_SymbolMap.h" // Debugger
#include "../Debugger/Debugger_BreakPoints.h"

#include "Boot_DOL.h"
#include "Boot.h"
#include "../Host.h"
#include "../VolumeHandler.h"
#include "../PatchEngine.h"
#include "../PowerPC/SignatureDB.h"
#include "../PowerPC/SymbolDB.h"
#include "../MemTools.h"

#include "VolumeCreator.h" // DiscIO

void CBoot::Load_FST(bool _bIsWii)
{
	if (VolumeHandler::IsValid())
	{
		// copy first 20 bytes of disc to start of Mem 1
		VolumeHandler::ReadToPtr(Memory::GetPointer(0x80000000), 0, 0x20);		

		// copy of game id
		Memory::Write_U32(Memory::Read_U32(0x80000000), 0x80003180);
		
		u32 shift = 0;
		if (_bIsWii)
			shift = 2;

		u32 fstOffset  = VolumeHandler::Read32(0x0424) << shift;
		u32 fstSize    = VolumeHandler::Read32(0x0428) << shift;
		u32 maxFstSize = VolumeHandler::Read32(0x042c) << shift;

		u32 arenaHigh = 0x817FFFF4 - maxFstSize;
		Memory::Write_U32(arenaHigh, 0x00000034);

		// load FST
		VolumeHandler::ReadToPtr(Memory::GetPointer(arenaHigh), fstOffset, fstSize);
		Memory::Write_U32(arenaHigh, 0x00000038);
		Memory::Write_U32(maxFstSize, 0x0000003c);		
	}	
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
	return FULL_MAPS_DIR + Core::GetStartupParameter().GetUniqueID() + ".map";
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
			BuildCompleteFilename(strMapFilename, "maps", std::string(_gameID) + ".map");
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


//////////////////////////////////////////////////////////////////////////////////////////
// Load a GC or Wii BIOS file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
bool CBoot::Load_BIOS(const std::string& _rBiosFilename)
{
    bool bResult = false;
    Common::IMappedFile* pFile = Common::IMappedFile::CreateMappedFileDEPRECATED();
    if (pFile->Open(_rBiosFilename.c_str()))
    {
        if (pFile->GetSize() >= 1024*1024*2)
        {
			// Write it to memory
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
/////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Third boot step after BootManager and Core. See Call schedule in BootManager.cpp
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
bool CBoot::BootUp(const SCoreStartupParameter& _StartupPara)
{
    const bool bDebugIsoBootup = false;

	g_symbolDB.Clear();
    VideoInterface::PreInit(_StartupPara.bNTSC);
    switch (_StartupPara.m_BootType)
    {
    // GCM and Wii
    // ===================================================================================
    case SCoreStartupParameter::BOOT_ISO:
        {
            DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(_StartupPara.m_strFilename);
            if (pVolume == NULL)
                break;

            bool isoWii = DiscIO::IsVolumeWiiDisc(pVolume);           
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

			// Use HLE BIOS or not
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
				// If we can't load the BIOS file we use the HLE BIOS instead
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

			/* Try to load the symbol map if there is one, and then scan it for
			   and eventually replace code */
            if (LoadMapFromFilename(_StartupPara.m_strFilename, gameID))
                HLE::PatchFunctions();

			// We don't need the volume any more
			delete pVolume;
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
			if(!File::Exists(_StartupPara.m_strFilename.c_str()))
			{
				PanicAlert("The file you specified (%s) does not exists",
					_StartupPara.m_strFilename.c_str());
				return false;
			}

			// Check if we have gotten a Wii file or not
			bool elfWii = IsElfWii(_StartupPara.m_strFilename.c_str());
			if (elfWii != Core::GetStartupParameter().bWii)
			{
				PanicAlert("Warning - starting ELF in wrong console mode!");
			}

			// stop apploader from running when BIOS boots
			VolumeHandler::SetVolumeName("");			

			if (elfWii)
			{                              
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

			// load image or create virtual drive from directory
			if (!_StartupPara.m_strDVDRoot.empty())
				VolumeHandler::SetVolumeDirectory(_StartupPara.m_strDVDRoot, elfWii);
			else if (!_StartupPara.m_strDefaultGCM.empty())
				VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM);
			else
				VolumeHandler::SetVolumeDirectory(_StartupPara.m_strFilename, elfWii);

			DVDInterface::SetDiscInside(VolumeHandler::IsValid());

			Load_FST(elfWii);			
			
            Boot_ELF(_StartupPara.m_strFilename.c_str()); 
            UpdateDebugger_MapLoaded();
			BreakPoints::AddAutoBreakpoints();
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
