// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// File description
// -------------
// Purpose of this file: Collect boot settings for Core::Init()

// Call sequence: This file has one of the first function called when a game is booted,
// the boot sequence in the code is:

// DolphinWX:    FrameTools.cpp         StartGame
// Core          BootManager.cpp        BootCore
//               Core.cpp               Init                     Thread creation
//                                      EmuThread                Calls CBoot::BootUp
//               Boot.cpp               CBoot::BootUp()
//                                      CBoot::EmulatedBS2_Wii() / GC() or Load_BS2()


// Includes
// ----------------
#include <string>
#include <vector>

#include "CommonTypes.h"
#include "CommonPaths.h"
#include "IniFile.h"
#include "BootManager.h"
#include "Volume.h"
#include "VolumeCreator.h"
#include "ConfigManager.h"
#include "SysConf.h"
#include "Core.h"
#include "Host.h"
#include "VideoBackendBase.h"
#include "Movie.h"
#include "NetPlayProto.h"

namespace BootManager
{

// TODO this is an ugly hack which allows us to restore values trampled by per-game settings
// Apply fire liberally
struct ConfigCache
{
	bool valid, bCPUThread, bSkipIdle, bEnableFPRF, bMMU, bDCBZOFF, m_EnableJIT, bDSPThread,
		bVBeamSpeedHack, bSyncGPU, bFastDiscSpeed, bMergeBlocks, bDSPHLE, bHLE_BS2, bTLBHack, bUseFPS;
	int iCPUCore, Volume;
	unsigned int framelimit;
	TEXIDevices m_EXIDevice[2];
	std::string strBackend, sBackend;
};
static ConfigCache config_cache;

// Boot the ISO or file
bool BootCore(const std::string& _rFilename)
{
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	// Use custom settings for debugging mode
	Host_SetStartupDebuggingParameters();

	StartUp.m_BootType = SCoreStartupParameter::BOOT_ISO;
	StartUp.m_strFilename = _rFilename;
	SConfig::GetInstance().m_LastFilename = _rFilename;
	SConfig::GetInstance().SaveSettings();
	StartUp.bRunCompareClient = false;
	StartUp.bRunCompareServer = false;

	StartUp.hInstance = Host_GetInstance();

	// If for example the ISO file is bad we return here
	if (!StartUp.AutoSetup(SCoreStartupParameter::BOOT_DEFAULT))
		return false;

	// Load game specific settings
	std::string unique_id = StartUp.GetUniqueID();
	std::string revision_specific = StartUp.m_strRevisionSpecificUniqueID;
	StartUp.m_strGameIniDefault = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + unique_id + ".ini";
	if (revision_specific != "")
		StartUp.m_strGameIniDefaultRevisionSpecific = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + revision_specific + ".ini";
	else
		StartUp.m_strGameIniDefaultRevisionSpecific = "";
	StartUp.m_strGameIniLocal = File::GetUserPath(D_GAMESETTINGS_IDX) + unique_id + ".ini";

	if (unique_id.size() == 6)
	{
		IniFile game_ini = StartUp.LoadGameIni();

		config_cache.valid = true;
		config_cache.bCPUThread = StartUp.bCPUThread;
		config_cache.bSkipIdle = StartUp.bSkipIdle;
		config_cache.iCPUCore = StartUp.iCPUCore;
		config_cache.bEnableFPRF = StartUp.bEnableFPRF;
		config_cache.bMMU = StartUp.bMMU;
		config_cache.bDCBZOFF = StartUp.bDCBZOFF;
		config_cache.bTLBHack = StartUp.bTLBHack;
		config_cache.bVBeamSpeedHack = StartUp.bVBeamSpeedHack;
		config_cache.bSyncGPU = StartUp.bSyncGPU;
		config_cache.bFastDiscSpeed = StartUp.bFastDiscSpeed;
		config_cache.bMergeBlocks = StartUp.bMergeBlocks;
		config_cache.bDSPHLE = StartUp.bDSPHLE;
		config_cache.strBackend = StartUp.m_strVideoBackend;
		config_cache.bHLE_BS2 = StartUp.bHLE_BS2;
		config_cache.m_EnableJIT = SConfig::GetInstance().m_EnableJIT;
		config_cache.bDSPThread = StartUp.bDSPThread;
		config_cache.m_EXIDevice[0] = SConfig::GetInstance().m_EXIDevice[0];
		config_cache.m_EXIDevice[1] = SConfig::GetInstance().m_EXIDevice[1];
		config_cache.Volume = SConfig::GetInstance().m_Volume;
		config_cache.sBackend = SConfig::GetInstance().sBackend;
		config_cache.framelimit = SConfig::GetInstance().m_Framelimit;
		config_cache.bUseFPS = SConfig::GetInstance().b_UseFPS;

		// General settings
		game_ini.Get("Core", "CPUThread",			&StartUp.bCPUThread, StartUp.bCPUThread);
		game_ini.Get("Core", "SkipIdle",			&StartUp.bSkipIdle, StartUp.bSkipIdle);
		game_ini.Get("Core", "EnableFPRF",			&StartUp.bEnableFPRF, StartUp.bEnableFPRF);
		game_ini.Get("Core", "MMU",					&StartUp.bMMU, StartUp.bMMU);
		game_ini.Get("Core", "TLBHack",				&StartUp.bTLBHack, StartUp.bTLBHack);
		game_ini.Get("Core", "DCBZ",				&StartUp.bDCBZOFF, StartUp.bDCBZOFF);
		game_ini.Get("Core", "VBeam",				&StartUp.bVBeamSpeedHack, StartUp.bVBeamSpeedHack);
		game_ini.Get("Core", "SyncGPU",				&StartUp.bSyncGPU, StartUp.bSyncGPU);
		game_ini.Get("Core", "FastDiscSpeed",		&StartUp.bFastDiscSpeed, StartUp.bFastDiscSpeed);
		game_ini.Get("Core", "BlockMerging",		&StartUp.bMergeBlocks, StartUp.bMergeBlocks);
		game_ini.Get("Core", "DSPHLE",				&StartUp.bDSPHLE, StartUp.bDSPHLE);
		game_ini.Get("Core", "DSPThread",			&StartUp.bDSPThread, StartUp.bDSPThread);
		game_ini.Get("Core", "GFXBackend", &StartUp.m_strVideoBackend, StartUp.m_strVideoBackend.c_str());
		game_ini.Get("Core", "CPUCore",				&StartUp.iCPUCore, StartUp.iCPUCore);
		game_ini.Get("Core", "HLE_BS2",				&StartUp.bHLE_BS2, StartUp.bHLE_BS2);
		game_ini.Get("Core", "FrameLimit",			&SConfig::GetInstance().m_Framelimit, SConfig::GetInstance().m_Framelimit);
		game_ini.Get("Core", "UseFPS",				&SConfig::GetInstance().b_UseFPS,SConfig::GetInstance().b_UseFPS);
		game_ini.Get("DSP", "Volume",				&SConfig::GetInstance().m_Volume, SConfig::GetInstance().m_Volume);
		game_ini.Get("DSP", "EnableJIT",			&SConfig::GetInstance().m_EnableJIT, SConfig::GetInstance().m_EnableJIT);
		game_ini.Get("DSP", "Backend",				&SConfig::GetInstance().sBackend, SConfig::GetInstance().sBackend.c_str());
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);

		// Wii settings
		if (StartUp.bWii)
		{
			// Flush possible changes to SYSCONF to file
			SConfig::GetInstance().m_SYSCONF->Save();
		}
	}

	// movie settings
	if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
	{
		StartUp.bCPUThread = Movie::IsDualCore();
		StartUp.bSkipIdle = Movie::IsSkipIdle();
		StartUp.bDSPHLE = Movie::IsDSPHLE();
		StartUp.bProgressive = Movie::IsProgressive();
		StartUp.bFastDiscSpeed = Movie::IsFastDiscSpeed();
		StartUp.iCPUCore = Movie::GetCPUMode();
		StartUp.bSyncGPU = Movie::IsSyncGPU();
		if (Movie::IsUsingMemcard() && Movie::IsStartingFromClearSave() && !StartUp.bWii)
		{
			if (File::Exists(File::GetUserPath(D_GCUSER_IDX) + "Movie.raw"))
				File::Delete(File::GetUserPath(D_GCUSER_IDX) + "Movie.raw");
		}
	}

	if (NetPlay::IsNetPlayRunning())
	{
		StartUp.bCPUThread = g_NetPlaySettings.m_CPUthread;
		StartUp.bDSPHLE = g_NetPlaySettings.m_DSPHLE;
		StartUp.bEnableMemcardSaving = g_NetPlaySettings.m_WriteToMemcard;
		SConfig::GetInstance().m_EnableJIT = g_NetPlaySettings.m_DSPEnableJIT;
		SConfig::GetInstance().m_EXIDevice[0] = g_NetPlaySettings.m_EXIDevice[0];
		SConfig::GetInstance().m_EXIDevice[1] = g_NetPlaySettings.m_EXIDevice[1];
	}

	// Run the game
	// Init the core
	if (!Core::Init())
	{
		PanicAlertT("Couldn't init the core.\nCheck your configuration.");
		return false;
	}

	return true;
}

void Stop()
{
	Core::Stop();

	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	StartUp.m_strUniqueID = "00000000";
	if (config_cache.valid)
	{
		config_cache.valid = false;
		StartUp.bCPUThread = config_cache.bCPUThread;
		StartUp.bSkipIdle = config_cache.bSkipIdle;
		StartUp.iCPUCore = config_cache.iCPUCore;
		StartUp.bEnableFPRF = config_cache.bEnableFPRF;
		StartUp.bMMU = config_cache.bMMU;
		StartUp.bDCBZOFF = config_cache.bDCBZOFF;
		StartUp.bTLBHack = config_cache.bTLBHack;
		StartUp.bVBeamSpeedHack = config_cache.bVBeamSpeedHack;
		StartUp.bSyncGPU = config_cache.bSyncGPU;
		StartUp.bFastDiscSpeed = config_cache.bFastDiscSpeed;
		StartUp.bMergeBlocks = config_cache.bMergeBlocks;
		StartUp.bDSPHLE = config_cache.bDSPHLE;
		StartUp.bDSPThread = config_cache.bDSPThread;
		StartUp.m_strVideoBackend = config_cache.strBackend;
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);
		StartUp.bHLE_BS2 = config_cache.bHLE_BS2;
		SConfig::GetInstance().m_Framelimit = config_cache.framelimit;
		SConfig::GetInstance().b_UseFPS = config_cache.bUseFPS;
		SConfig::GetInstance().m_EnableJIT = config_cache.m_EnableJIT;
		SConfig::GetInstance().m_EXIDevice[0] = config_cache.m_EXIDevice[0];
		SConfig::GetInstance().m_EXIDevice[1] = config_cache.m_EXIDevice[1];
		SConfig::GetInstance().m_Volume = config_cache.Volume;
		SConfig::GetInstance().sBackend = config_cache.sBackend;
	}
}

} // namespace
