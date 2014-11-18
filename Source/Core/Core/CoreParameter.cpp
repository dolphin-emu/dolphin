// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>

#include "Common/CDUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h" // for bWii
#include "Core/CoreParameter.h"
#include "Core/Boot/Boot.h"
#include "Core/Boot/Boot_DOL.h"
#include "Core/FifoPlayer/FifoDataFile.h"

#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/VolumeCreator.h"

SCoreStartupParameter::SCoreStartupParameter()
: bEnableDebugging(false), bAutomaticStart(false), bBootToPause(false),
  bJITNoBlockCache(false), bJITNoBlockLinking(false),
  bJITOff(false),
  bJITLoadStoreOff(false), bJITLoadStorelXzOff(false),
  bJITLoadStorelwzOff(false), bJITLoadStorelbzxOff(false),
  bJITLoadStoreFloatingOff(false), bJITLoadStorePairedOff(false),
  bJITFloatingPointOff(false), bJITIntegerOff(false),
  bJITPairedOff(false), bJITSystemRegistersOff(false),
  bJITBranchOff(false),
  bJITILTimeProfiling(false), bJITILOutputIR(false),
  bFPRF(false),
  bCPUThread(true), bDSPThread(false), bDSPHLE(true),
  bSkipIdle(true), bNTSC(false), bForceNTSCJ(false),
  bHLE_BS2(true), bEnableCheats(false),
  bMergeBlocks(false), bEnableMemcardSaving(true),
  bDPL2Decoder(false), iLatency(14),
  bRunCompareServer(false), bRunCompareClient(false),
  bBAT(false), bMMU(false), bDCBZOFF(false),
  iBBDumpPort(0), bVBeamSpeedHack(false),
  bSyncGPU(false), bFastDiscSpeed(false),
  SelectedLanguage(0), bWii(false),
  bConfirmStop(false), bHideCursor(false),
  bAutoHideCursor(false), bUsePanicHandlers(true), bOnScreenDisplayMessages(true),
  iRenderWindowXPos(-1), iRenderWindowYPos(-1),
  iRenderWindowWidth(640), iRenderWindowHeight(480),
  bRenderWindowAutoSize(false), bKeepWindowOnTop(false),
  bFullscreen(false), bRenderToMain(false),
  bProgressive(false), bDisableScreenSaver(false),
  iPosX(100), iPosY(100), iWidth(800), iHeight(600),
  bLoopFifoReplay(true)
{
	LoadDefaults();
}

void SCoreStartupParameter::LoadDefaults()
{
	bEnableDebugging = false;
	bAutomaticStart = false;
	bBootToPause = false;

	#ifdef USE_GDBSTUB
	iGDBPort = -1;
	#endif

	iCPUCore = CORE_JIT64;
	bCPUThread = false;
	bSkipIdle = false;
	bRunCompareServer = false;
	bDSPHLE = true;
	bDSPThread = true;
	bFastmem = true;
	bFPRF = false;
	bBAT = false;
	bMMU = false;
	bDCBZOFF = false;
	iBBDumpPort = -1;
	bVBeamSpeedHack = false;
	bSyncGPU = false;
	bFastDiscSpeed = false;
	bMergeBlocks = false;
	bEnableMemcardSaving = true;
	SelectedLanguage = 0;
	bWii = false;
	bDPL2Decoder = false;
	iLatency = 14;

	iPosX = 100;
	iPosY = 100;
	iWidth = 800;
	iHeight = 600;

	bLoopFifoReplay = true;

	bJITOff = false; // debugger only settings
	bJITLoadStoreOff = false;
	bJITLoadStoreFloatingOff = false;
	bJITLoadStorePairedOff = false; // XXX not 64-bit clean
	bJITFloatingPointOff = false;
	bJITIntegerOff = false;
	bJITPairedOff = false;
	bJITSystemRegistersOff = false;

	m_strName = "NONE";
	m_strUniqueID = "00000000";
}

bool SCoreStartupParameter::AutoSetup(EBootBS2 _BootBS2)
{
	std::string Region(EUR_DIR);

	switch (_BootBS2)
	{
	case BOOT_DEFAULT:
		{
			bool bootDrive = cdio_is_cdrom(m_strFilename);
			// Check if the file exist, we may have gotten it from a --elf command line
			// that gave an incorrect file name
			if (!bootDrive && !File::Exists(m_strFilename))
			{
				PanicAlertT("The specified file \"%s\" does not exist", m_strFilename.c_str());
				return false;
			}

			std::string Extension;
			SplitPath(m_strFilename, nullptr, nullptr, &Extension);
			if (!strcasecmp(Extension.c_str(), ".gcm") ||
				!strcasecmp(Extension.c_str(), ".iso") ||
				!strcasecmp(Extension.c_str(), ".wbfs") ||
				!strcasecmp(Extension.c_str(), ".ciso") ||
				!strcasecmp(Extension.c_str(), ".gcz") ||
				bootDrive)
			{
				m_BootType = BOOT_ISO;
				std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(m_strFilename));
				if (pVolume == nullptr)
				{
					if (bootDrive)
						PanicAlertT("Could not read \"%s\".  "
								"There is no disc in the drive, or it is not a GC/Wii backup.  "
								"Please note that original GameCube and Wii discs cannot be read "
								"by most PC DVD drives.", m_strFilename.c_str());
					else
						PanicAlertT("\"%s\" is an invalid GCM/ISO file, or is not a GC/Wii ISO.",
								m_strFilename.c_str());
					return false;
				}
				m_strName = pVolume->GetName();
				m_strUniqueID = pVolume->GetUniqueID();
				m_strRevisionSpecificUniqueID = pVolume->GetRevisionSpecificUniqueID();

				// Check if we have a Wii disc
				bWii = DiscIO::IsVolumeWiiDisc(pVolume.get());
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

				case DiscIO::IVolume::COUNTRY_AUSTRALIA:
				case DiscIO::IVolume::COUNTRY_EUROPE:
				case DiscIO::IVolume::COUNTRY_FRANCE:
				case DiscIO::IVolume::COUNTRY_INTERNATIONAL:
				case DiscIO::IVolume::COUNTRY_ITALY:
				case DiscIO::IVolume::COUNTRY_NETHERLANDS:
				case DiscIO::IVolume::COUNTRY_RUSSIA:
				case DiscIO::IVolume::COUNTRY_SPAIN:
					bNTSC = false;
					Region = EUR_DIR;
					break;

				case DiscIO::IVolume::COUNTRY_UNKNOWN:
				default:
					if (PanicYesNoT("Your GCM/ISO file seems to be invalid (invalid country)."
								   "\nContinue with PAL region?"))
					{
						bNTSC = false;
						Region = EUR_DIR;
						break;
					}
					else
					{
						return false;
					}
				}
			}
			else if (!strcasecmp(Extension.c_str(), ".elf"))
			{
				bWii = CBoot::IsElfWii(m_strFilename);
				Region = USA_DIR;
				m_BootType = BOOT_ELF;
				bNTSC = true;
			}
			else if (!strcasecmp(Extension.c_str(), ".dol"))
			{
				CDolLoader dolfile(m_strFilename);
				bWii = dolfile.IsWii();
				Region = USA_DIR;
				m_BootType = BOOT_DOL;
				bNTSC = true;
			}
			else if (!strcasecmp(Extension.c_str(), ".dff"))
			{
				bWii = true;
				Region = USA_DIR;
				bNTSC = true;
				m_BootType = BOOT_DFF;

				std::unique_ptr<FifoDataFile> ddfFile(FifoDataFile::Load(m_strFilename, true));

				if (ddfFile)
				{
					bWii = ddfFile->GetIsWii();
				}
			}
			else if (DiscIO::CNANDContentManager::Access().GetNANDLoader(m_strFilename).IsValid())
			{
				std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(m_strFilename));
				const DiscIO::INANDContentLoader& ContentLoader = DiscIO::CNANDContentManager::Access().GetNANDLoader(m_strFilename);

				if (ContentLoader.GetContentByIndex(ContentLoader.GetBootIndex()) == nullptr)
				{
					//WAD is valid yet cannot be booted. Install instead.
					u64 installed = DiscIO::CNANDContentManager::Access().Install_WiiWAD(m_strFilename);
					if (installed)
						SuccessAlertT("The WAD has been installed successfully");
					return false; //do not boot
				}

				switch (ContentLoader.GetCountry())
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

				case DiscIO::IVolume::COUNTRY_AUSTRALIA:
				case DiscIO::IVolume::COUNTRY_EUROPE:
				case DiscIO::IVolume::COUNTRY_FRANCE:
				case DiscIO::IVolume::COUNTRY_INTERNATIONAL:
				case DiscIO::IVolume::COUNTRY_ITALY:
				case DiscIO::IVolume::COUNTRY_RUSSIA:
					bNTSC = false;
					Region = EUR_DIR;
					break;

				case DiscIO::IVolume::COUNTRY_UNKNOWN:
				default:
					bNTSC = false;
					Region = EUR_DIR;
						break;
				}

				bWii = true;
				m_BootType = BOOT_WII_NAND;

				if (pVolume)
				{
					m_strName = pVolume->GetName();
					m_strUniqueID = pVolume->GetUniqueID();
				}
				else
				{
					// null pVolume means that we are loading from nand folder (Most Likely Wii Menu)
					// if this is the second boot we would be using the Name and id of the last title
					m_strName.clear();
					m_strUniqueID.clear();
				}

				// Use the TitleIDhex for name and/or unique ID if launching from nand folder
				// or if it is not ascii characters (specifically sysmenu could potentially apply to other things)
				std::string titleidstr = StringFromFormat("%016" PRIx64, ContentLoader.GetTitleID());

				if (m_strName.empty())
				{
					m_strName = titleidstr;
				}
				if (m_strUniqueID.empty())
				{
					m_strUniqueID = titleidstr;
				}
			}
			else
			{
				PanicAlertT("Could not recognize ISO file %s", m_strFilename.c_str());
				return false;
			}
		}
		break;

	case BOOT_BS2_USA:
		Region = USA_DIR;
		m_strFilename.clear();
		bNTSC = true;
		break;

	case BOOT_BS2_JAP:
		Region = JAP_DIR;
		m_strFilename.clear();
		bNTSC = true;
		break;

	case BOOT_BS2_EUR:
		Region = EUR_DIR;
		m_strFilename.clear();
		bNTSC = false;
		break;
	}

	// Setup paths
	CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardA, Region, true);
	CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardB, Region, false);
	m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
	if (!bWii)
	{
		if (!bHLE_BS2)
		{
			m_strBootROM = File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + Region + DIR_SEP GC_IPL;
			if (!File::Exists(m_strBootROM))
				m_strBootROM = File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + Region + DIR_SEP GC_IPL;

			if (!File::Exists(m_strBootROM))
			{
				WARN_LOG(BOOT, "Bootrom file %s not found - using HLE.", m_strBootROM.c_str());
				bHLE_BS2 = true;
			}
		}
	}
	else if (bWii && !bHLE_BS2)
	{
		WARN_LOG(BOOT, "GC bootrom file will not be loaded for Wii mode.");
		bHLE_BS2 = true;
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
		memcardPath = File::GetUserPath(D_GCUSER_IDX) + defaultFilename + ext;
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
			if (File::Exists(filename))
			{
				// If the old file exists we are polite and ask if we should copy it
				std::string oldFilename = filename;
				filename.replace(filename.size()-4, 4, ext);
				if (PanicYesNoT("Memory Card filename in Slot %c is incorrect\n"
					"Region not specified\n\n"
					"Slot %c path was changed to\n"
					"%s\n"
					"Would you like to copy the old file to this new location?\n",
					isSlotA ? 'A':'B', isSlotA ? 'A':'B', filename.c_str()))
				{
					if (!File::Copy(oldFilename, filename))
						PanicAlertT("Copy failed");
				}
			}
			memcardPath = filename; // Always correct the path!
		}
		else if (region.compare(gameRegion) != 0)
		{
			// filename has region, but it's not == gameRegion
			// Just set the correct filename, the EXI Device will create it if it doesn't exist
			memcardPath = filename.replace(filename.size()-ext.size(), ext.size(), ext);
		}
	}
}

IniFile SCoreStartupParameter::LoadGameIni() const
{
	IniFile game_ini;
	game_ini.Load(m_strGameIniDefault);
	if (m_strGameIniDefaultRevisionSpecific != "")
		game_ini.Load(m_strGameIniDefaultRevisionSpecific, true);
	game_ini.Load(m_strGameIniLocal, true);
	return game_ini;
}

IniFile SCoreStartupParameter::LoadDefaultGameIni() const
{
	IniFile game_ini;
	game_ini.Load(m_strGameIniDefault);
	if (m_strGameIniDefaultRevisionSpecific != "")
		game_ini.Load(m_strGameIniDefaultRevisionSpecific, true);
	return game_ini;
}

IniFile SCoreStartupParameter::LoadLocalGameIni() const
{
	IniFile game_ini;
	game_ini.Load(m_strGameIniLocal);
	return game_ini;
}
