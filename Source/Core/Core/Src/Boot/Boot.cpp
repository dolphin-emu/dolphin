// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "Common.h" // Common
#include "StringUtil.h"
#include "FileUtil.h"
#include "MathUtil.h"

#include "../HLE/HLE.h" // Core
#include "../PowerPC/PowerPC.h"
#include "../PowerPC/PPCAnalyst.h"
#include "../Core.h"
#include "../HW/HW.h"
#include "../HW/EXI_DeviceIPL.h"
#include "../HW/Memmap.h"
#include "../HW/ProcessorInterface.h"
#include "../HW/DVDInterface.h"
#include "../HW/VideoInterface.h"
#include "../HW/CPU.h"
#include "../IPC_HLE/WII_IPC_HLE.h"

#include "../Debugger/Debugger_SymbolMap.h" // Debugger

#include "Boot_DOL.h"
#include "Boot.h"
#include "../Host.h"
#include "../VolumeHandler.h"
#include "../PatchEngine.h"
#include "../PowerPC/SignatureDB.h"
#include "../PowerPC/PPCSymbolDB.h"
#include "../MemTools.h"

#include "../ConfigManager.h"
#include "VolumeCreator.h" // DiscIO
#include "NANDContentLoader.h"
#include "Hash.h"
#include "CommonPaths.h"

void CBoot::Load_FST(bool _bIsWii)
{
	if (!VolumeHandler::IsValid())
		return;

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

	u32 arenaHigh = ROUND_DOWN(0x817FFFFF - maxFstSize, 0x20);
	Memory::Write_U32(arenaHigh, 0x00000034);

	// load FST
	VolumeHandler::ReadToPtr(Memory::GetPointer(arenaHigh), fstOffset, fstSize);
	Memory::Write_U32(arenaHigh, 0x00000038);
	Memory::Write_U32(maxFstSize, 0x0000003c);
}

void CBoot::UpdateDebugger_MapLoaded(const char *_gameID)
{
	Host_NotifyMapLoaded();
}

std::string CBoot::GenerateMapFilename()
{
	SCoreStartupParameter& _StartupPara = SConfig::GetInstance().m_LocalCoreStartupParameter;
	switch (_StartupPara.m_BootType)
	{
	case SCoreStartupParameter::BOOT_WII_NAND:
	{
		const DiscIO::INANDContentLoader& Loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(_StartupPara.m_strFilename);
		if (Loader.IsValid())
		{
			u64 TitleID = Loader.GetTitleID();
			char tmpBuffer[32];
			sprintf(tmpBuffer, "%08x_%08x", (u32)(TitleID >> 32) & 0xFFFFFFFF , (u32)TitleID & 0xFFFFFFFF );
			return File::GetUserPath(D_MAPS_IDX) + std::string(tmpBuffer) + ".map";
		}
		break;
	}

	case SCoreStartupParameter::BOOT_ELF:
	case SCoreStartupParameter::BOOT_DOL:
		return _StartupPara.m_strFilename.substr(0, _StartupPara.m_strFilename.size()-4) + ".map";
	default:
		return File::GetUserPath(D_MAPS_IDX) + _StartupPara.GetUniqueID() + ".map";
	}

	return std::string("unknown map");
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

// If ipl.bin is not found, this function does *some* of what BS1 does: 
// loading IPL(BS2) and jumping to it.
// It does not initialize the hardware or anything else like BS1 does.
bool CBoot::Load_BS2(const std::string& _rBootROMFilename)
{
	const u32 USA = 0x1FCE3FD6;
	const u32 USA_v1_1 = 0x4D5935D1;
	const u32 JAP = 0x87424396;
	const u32 PAL = 0xA0EA7341;
	//const u32 PanasonicQJ = 0xAEA8265C;
	//const u32 PanasonicQU = 0x94015753;

	// Load the whole ROM dump
	std::string data;
	if (!File::ReadFileToString(false, _rBootROMFilename.c_str(), data))
		return false;

	u32 ipl_hash = HashAdler32((const u8*)data.data(), data.size());
	std::string ipl_region;
	switch (ipl_hash)
	{
	case USA:
	case USA_v1_1:
		ipl_region = USA_DIR;
		break;
	case JAP:
		ipl_region = JAP_DIR;
		break;
	case PAL:
		ipl_region = EUR_DIR;
		break;
	default:
		PanicAlert("IPL with unknown hash %x", ipl_hash);
		break;
	}

	std::string BootRegion = _rBootROMFilename.substr(_rBootROMFilename.find_last_of(DIR_SEP) - 3, 3);
	if (BootRegion != ipl_region)
		PanicAlert("%s IPL found in %s directory. The disc may not be recognized", ipl_region.c_str(), BootRegion.c_str());

	// Run the descrambler over the encrypted section containing BS1/BS2
	CEXIIPL::Descrambler((u8*)data.data()+0x100, 0x1AFE00);

	Memory::WriteBigEData((const u8*)data.data() + 0x100, 0x81200000, 0x700);
	Memory::WriteBigEData((const u8*)data.data() + 0x820, 0x81300000, 0x1AFE00);
	PC = 0x81200000;
	return true;
}


// Third boot step after BootManager and Core. See Call schedule in BootManager.cpp
bool CBoot::BootUp()
{
	SCoreStartupParameter& _StartupPara = 
	SConfig::GetInstance().m_LocalCoreStartupParameter;

	NOTICE_LOG(BOOT, "Booting %s", _StartupPara.m_strFilename.c_str());

	// HLE jump to loader (homebrew).  Disabled when Gecko is active as it interferes with the code handler
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats)
	{
		HLE::Patch(0x80001800, "HBReload");
		const u8 stubstr[] = { 'S', 'T', 'U', 'B', 'H', 'A', 'X', 'X' };
		Memory::WriteBigEData(stubstr, 0x80001804, 8);
	}

	g_symbolDB.Clear();
	VideoInterface::Preset(_StartupPara.bNTSC);
	switch (_StartupPara.m_BootType)
	{
	// GCM and Wii
	case SCoreStartupParameter::BOOT_ISO:
	{
		DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(_StartupPara.m_strFilename);
		if (pVolume == NULL)
			break;

		bool isoWii = DiscIO::IsVolumeWiiDisc(pVolume);
		if (isoWii != _StartupPara.bWii)
		{
			PanicAlertT("Warning - starting ISO in wrong console mode!");
		}

		char gameID[7];
		memcpy(gameID, pVolume->GetUniqueID().c_str(), 6);
		gameID[6] = 0;

		// setup the map from ISOFile ID
		VolumeHandler::SetVolumeName(_StartupPara.m_strFilename);

		std::string unique_id = VolumeHandler::GetVolume()->GetUniqueID();
		if (unique_id.size() >= 4)
			VideoInterface::SetRegionReg(unique_id.at(3));

		DVDInterface::SetDiscInside(VolumeHandler::IsValid());

		u32 _TMDsz = 0x208;
		u8* _pTMD = new u8[_TMDsz];
		pVolume->GetTMD(_pTMD, &_TMDsz);
		if (_TMDsz)
		{
			WII_IPC_HLE_Interface::ES_DIVerify(_pTMD, _TMDsz);
		}
		delete []_pTMD;


		_StartupPara.bWii = VolumeHandler::IsWii();

		// HLE BS2 or not
		if (_StartupPara.bHLE_BS2)
		{
			EmulatedBS2(_StartupPara.bWii);
		}
		else if (!Load_BS2(_StartupPara.m_strBootROM))
		{
			// If we can't load the bootrom file we HLE it instead
			EmulatedBS2(_StartupPara.bWii);
		}

		// Scan for common HLE functions
		if (_StartupPara.bSkipIdle && !_StartupPara.bEnableDebugging)
		{
			PPCAnalyst::FindFunctions(0x80004000, 0x811fffff, &g_symbolDB);
			SignatureDB db;
			if (db.Load((File::GetSysDirectory() + TOTALDB).c_str()))
			{
				db.Apply(&g_symbolDB);
				HLE::PatchFunctions();
				db.Clear();
			}
		}

		/* Try to load the symbol map if there is one, and then scan it for
			and eventually replace code */
		if (LoadMapFromFilename(_StartupPara.m_strFilename, gameID))
			HLE::PatchFunctions();

		// We don't need the volume any more
		delete pVolume;
		break;
	}

	// DOL
	case SCoreStartupParameter::BOOT_DOL:
	{
		CDolLoader dolLoader(_StartupPara.m_strFilename.c_str());
		// Check if we have gotten a Wii file or not
		bool dolWii = dolLoader.IsWii();
		if (dolWii != _StartupPara.bWii)
		{
			PanicAlertT("Warning - starting DOL in wrong console mode!");
		}

		bool BS2Success = false;

		if (dolWii)
		{
			BS2Success = EmulatedBS2(dolWii);
		}
		else if (!VolumeHandler::IsWii() && !_StartupPara.m_strDefaultGCM.empty())
		{
			VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM.c_str());
			BS2Success = EmulatedBS2(dolWii);
		}

		if (!_StartupPara.m_strDVDRoot.empty())
		{
			NOTICE_LOG(BOOT, "Setting DVDRoot %s", _StartupPara.m_strDVDRoot.c_str());
			VolumeHandler::SetVolumeDirectory(_StartupPara.m_strDVDRoot, dolWii, _StartupPara.m_strApploader, _StartupPara.m_strFilename);
			BS2Success = EmulatedBS2(dolWii);
		} 

		DVDInterface::SetDiscInside(VolumeHandler::IsValid());

		if (!BS2Success)
		{
			dolLoader.Load();
			PC = dolLoader.GetEntryPoint();
		}

		if (LoadMapFromFilename(_StartupPara.m_strFilename))
			HLE::PatchFunctions();

		break;
	}

	// ELF
	case SCoreStartupParameter::BOOT_ELF:
	{
		if(!File::Exists(_StartupPara.m_strFilename))
		{
			PanicAlertT("The file you specified (%s) does not exist",
				_StartupPara.m_strFilename.c_str());
			return false;
		}

		// Check if we have gotten a Wii file or not
		bool elfWii = IsElfWii(_StartupPara.m_strFilename.c_str());
		if (elfWii != _StartupPara.bWii)
		{
			PanicAlertT("Warning - starting ELF in wrong console mode!");
		}			

		bool BS2Success = false;

		if (elfWii)
		{
			BS2Success = EmulatedBS2(elfWii);
		}
		else if (!VolumeHandler::IsWii() && !_StartupPara.m_strDefaultGCM.empty())
		{
			VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM.c_str());
			BS2Success = EmulatedBS2(elfWii);
		}

		// load image or create virtual drive from directory
		if (!_StartupPara.m_strDVDRoot.empty())
		{
			NOTICE_LOG(BOOT, "Setting DVDRoot %s", _StartupPara.m_strDVDRoot.c_str());
			// TODO: auto-convert elf to dol, so we can load them :)
			VolumeHandler::SetVolumeDirectory(_StartupPara.m_strDVDRoot, elfWii);
			BS2Success = EmulatedBS2(elfWii);
		} 
		else if (!_StartupPara.m_strDefaultGCM.empty())
		{
			NOTICE_LOG(BOOT, "Loading default ISO %s", _StartupPara.m_strDefaultGCM.c_str());
			VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM);
		}
		else VolumeHandler::SetVolumeDirectory(_StartupPara.m_strFilename, elfWii);

		DVDInterface::SetDiscInside(VolumeHandler::IsValid());

		if (BS2Success)
		{
			HLE::PatchFunctions();
		}
		else // Poor man's bootup
		{
			Load_FST(elfWii);
			Boot_ELF(_StartupPara.m_strFilename.c_str()); 
		}
		UpdateDebugger_MapLoaded();
		Dolphin_Debugger::AddAutoBreakpoints();
		break;
	}

	// Wii WAD
	case SCoreStartupParameter::BOOT_WII_NAND:
		Boot_WiiWAD(_StartupPara.m_strFilename.c_str());

		if (LoadMapFromFilename(_StartupPara.m_strFilename))
			HLE::PatchFunctions();

		// load default image or create virtual drive from directory
		if (!_StartupPara.m_strDVDRoot.empty())
			VolumeHandler::SetVolumeDirectory(_StartupPara.m_strDVDRoot, true);
		else if (!_StartupPara.m_strDefaultGCM.empty())
			VolumeHandler::SetVolumeName(_StartupPara.m_strDefaultGCM);

		DVDInterface::SetDiscInside(VolumeHandler::IsValid());
		break;


	// Bootstrap 2 (AKA: Initial Program Loader, "BIOS")
	case SCoreStartupParameter::BOOT_BS2:
	{
		DVDInterface::SetDiscInside(VolumeHandler::IsValid());
		if (Load_BS2(_StartupPara.m_strBootROM))
		{
			if (LoadMapFromFilename(_StartupPara.m_strFilename))
				HLE::PatchFunctions();
		}
		else
		{
			return false;
		}
		break;
	}

	case SCoreStartupParameter::BOOT_DFF:
		// do nothing
		break;

	default:
	{
		PanicAlertT("Tried to load an unknown file type.");
		return false;
	}
	}
	Host_UpdateLogDisplay();
	return true;
}
