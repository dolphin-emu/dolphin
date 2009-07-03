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

#include "Common.h" // Common
#include "FileUtil.h"
#include "StringUtil.h"
#include "CDUtils.h"
#include "NANDContentLoader.h"

#include "VolumeCreator.h" // DiscIO

#include "Boot/Boot.h" // Core
#include "Boot/Boot_DOL.h"
#include "CoreParameter.h"
#include "ConfigManager.h"
#include "Core.h" // for bWii

SCoreStartupParameter::SCoreStartupParameter()
{
	LoadDefaults();
}

void SCoreStartupParameter::LoadDefaults()
{
	bEnableDebugging = false;
	bUseJIT = false;
	bUseDualCore = false;
	bSkipIdle = false;
	bRunCompareServer = false;
	bDSPThread = true;
	bLockThreads = true;
	bEnableFPRF = false;
	bWii = false;
	SelectedLanguage = 0;
	iTLBHack = 0;
	delete gameIni;
	gameIni = NULL;

	bJITOff = false; // debugger only settings
	bJITLoadStoreOff = false;
	bJITLoadStoreFloatingOff = false;
	bJITLoadStorePairedOff = false;
	bJITFloatingPointOff = false;
	bJITIntegerOff = false;
	bJITPairedOff = false;
	bJITSystemRegistersOff = false;

	m_strName = "NONE";
	m_strUniqueID = "00000000";
}

bool SCoreStartupParameter::AutoSetup(EBootBios _BootBios) 
{
	std::string Region(EUR_DIR);

	switch (_BootBios)
	{
	case BOOT_DEFAULT:
		{
			bool bootDrive = cdio_is_cdrom(m_strFilename.c_str());
			// Check if the file exist, we may have gotten it from a --elf command line
			// that gave an incorrect file name 
			if (!bootDrive && !File::Exists(m_strFilename.c_str()))
			{
				PanicAlert("The file you specified (%s) does not exists", m_strFilename.c_str());
				return false;
			}

			std::string Extension;
			SplitPath(m_strFilename, NULL, NULL, &Extension);
			if (!strcasecmp(Extension.c_str(), ".gcm") || 
				!strcasecmp(Extension.c_str(), ".iso") ||
				!strcasecmp(Extension.c_str(), ".gcz") ||
				bootDrive)
			{
				m_BootType = BOOT_ISO;
				DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(m_strFilename.c_str());
				if (pVolume == NULL)
				{
					PanicAlert(bootDrive ?
						"Disc in %s could not be read - no disc, or not a GC/Wii backup.\n"
						"Please note that original Gamecube and Wii discs cannot be read by most PC DVD drives.":
						"Your GCM/ISO file seems to be invalid, or not a GC/Wii ISO.", m_strFilename.c_str());
					return false;
				}
				m_strName = pVolume->GetName();
				m_strUniqueID = pVolume->GetUniqueID();

				// Check if we have a Wii disc
				bWii = DiscIO::IsVolumeWiiDisc(pVolume);
				switch (pVolume->GetCountry())
				{
				case DiscIO::IVolume::COUNTRY_USA:
					bNTSC = true;
					Region = USA_DIR; 
					break;

				case DiscIO::IVolume::COUNTRY_TAIWAN:
				case DiscIO::IVolume::COUNTRY_KOREA:
					// TODO: Should these have their own Region Dir?
				case DiscIO::IVolume::COUNTRY_JAPAN:
					bNTSC = true;
					Region = JAP_DIR; 
					break;

				case DiscIO::IVolume::COUNTRY_EUROPE:
				case DiscIO::IVolume::COUNTRY_FRANCE:
				case DiscIO::IVolume::COUNTRY_ITALY:
					bNTSC = false;
					Region = EUR_DIR; 
					break;

				default:
					if (PanicYesNo("Your GCM/ISO file seems to be invalid (invalid country)."
								   "\nContinue with PAL region?"))
					{
						bNTSC = false;
						Region = EUR_DIR; 
						break;
					}else return false;
				}

				delete pVolume;
			}
			else if (!strcasecmp(Extension.c_str(), ".elf"))
			{
				bWii = CBoot::IsElfWii(m_strFilename.c_str());
				Region = USA_DIR; 
				m_BootType = BOOT_ELF;
				bNTSC = true;
			}
			else if (!strcasecmp(Extension.c_str(), ".dol"))
			{
				bWii = CDolLoader::IsDolWii(m_strFilename.c_str());
				Region = USA_DIR; 
				m_BootType = BOOT_DOL;
				bNTSC = true;
			}
            else if (DiscIO::CNANDContentManager::Access().GetNANDLoader(m_strFilename).IsValid())
			{
				const DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(m_strFilename.c_str());
				const DiscIO::INANDContentLoader& ContentLoader = DiscIO::CNANDContentManager::Access().GetNANDLoader(m_strFilename);
		
				if (ContentLoader.GetContentByIndex(ContentLoader.GetBootIndex()) == NULL)
				{
					//WAD is valid yet cannot be booted. Install instead.
					bool installed = CBoot::Install_WiiWAD(m_strFilename.c_str());
					if (installed)
						SuccessAlert("The WAD has been installed successfully");
					return false; //do not boot
				}

				u64 TitleID = ContentLoader.GetTitleID();
				char* pTitleID = (char*)&TitleID;

				// NTSC or PAL
				if (pTitleID[0] == 'E' || pTitleID[0] == 'J')
					bNTSC = true;
				else
					bNTSC = false;

				bWii = true;
				Region = EUR_DIR;
				m_BootType = BOOT_WII_NAND;

				if (pVolume)
				{
					m_strName = pVolume->GetName();
					m_strUniqueID = pVolume->GetUniqueID();
				}

				delete pVolume;
			}
			else
			{
				PanicAlert("Could not recognize ISO file %s", m_strFilename.c_str());
				return false;
			}
		}
		break;

	case BOOT_BIOS_USA:
		Region = USA_DIR;
		m_strFilename.clear();
		bNTSC = true;
		break;

	case BOOT_BIOS_JAP:
		Region = JAP_DIR;
		m_strFilename.clear();
		bNTSC = true;
		break;

	case BOOT_BIOS_EUR:  
		Region = EUR_DIR;
		m_strFilename.clear();
		bNTSC = false;
		break;
	}

	// Setup paths
	CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardA, Region, true);
	CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardB, Region, false);
	m_strSRAM = GC_SRAM_FILE;
	if (!bWii)
	{
		m_strBios = File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + Region + DIR_SEP GC_IPL;
		if (!bHLEBios)
		{
			if (!File::Exists(m_strBios.c_str()))
			{
				WARN_LOG(BOOT, "BIOS file %s not found - using HLE.", m_strBios.c_str());
				bHLEBios = true;
			}
		}
	}
	else if (bWii && !bHLEBios)
	{
		WARN_LOG(BOOT, "GC BIOS file will not be loaded for Wii mode.");
		bHLEBios = true;
	}

	return true;
}

void SCoreStartupParameter::CheckMemcardPath(std::string& memcardPath, std::string gameRegion, bool isSlotA)
{
	std::string ext("." + gameRegion + ".raw");
	if (memcardPath.empty())
	{
		// Use default memcard path if there is no user defined name
		std::string defaultFilename = isSlotA ? GC_MEMCARDA : GC_MEMCARDB;
		memcardPath = FULL_GC_USER_DIR + defaultFilename + ext;
	}
	else
	{
		std::string filename = memcardPath;
		std::string region = filename.substr(filename.size()-7, 3);
		bool hasregion = false;
		hasregion |= region.compare(USA_DIR) == 0;
		hasregion |= region.compare(JAP_DIR) == 0;
		hasregion |= region.compare(EUR_DIR) == 0;
		if (!hasregion)
		{
			// filename doesn't have region in the extension
			if (File::Exists(filename.c_str()))
			{
				// If the old file exists we are polite and ask if we should copy it
				std::string oldFilename = filename;
				filename.replace(filename.size()-4, 4, ext);
				if (PanicYesNo("Memory Card filename in Slot %c is incorrect\n"
					"Region not specified\n\n"
					"Slot %c path was changed to\n"
					"%s\n"
					"Would you like to copy the old file to this new location?\n",
					isSlotA ? 'A':'B', isSlotA ? 'A':'B', filename.c_str()))
				{
					if (!File::Copy(oldFilename.c_str(), filename.c_str()))
						PanicAlert("Copy failed");
				}
			}
			memcardPath = filename; // Always correct the path!
		}
		else if (region.compare(gameRegion) != 0)
		{
			// filename has region, but it's not == gameRegion
			// Just set the correct filename, the EXI Device will create it if it doesn't exist
			memcardPath = filename.replace(filename.size()-ext.size(), ext.size(), ext);;
		}
	}
}
