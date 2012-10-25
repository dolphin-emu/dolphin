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

#include "Common.h"
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


namespace BootManager
{

// TODO this is an ugly hack which allows us to restore values trampled by per-game settings
// Apply fire liberally
struct ConfigCache
{
	bool valid, bCPUThread, bSkipIdle, bEnableFPRF, bMMU, bMMUBAT,
		bVBeam, bFastDiscSpeed, bMergeBlocks, bDSPHLE, bDisableWiimoteSpeaker;
	int iTLBHack;
	std::string strBackend;
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
	StartUp.bRunCompareClient = false;
	StartUp.bRunCompareServer = false;

	StartUp.hInstance = Host_GetInstance();

	// If for example the ISO file is bad we return here
	if (!StartUp.AutoSetup(SCoreStartupParameter::BOOT_DEFAULT)) return false;

	// Load game specific settings
	IniFile game_ini;
	std::string unique_id = StartUp.GetUniqueID();
	StartUp.m_strGameIni = File::GetUserPath(D_GAMECONFIG_IDX) + unique_id + ".ini";
	if (unique_id.size() == 6 && game_ini.Load(StartUp.m_strGameIni.c_str()))
	{
		config_cache.valid = true;
		config_cache.bCPUThread = StartUp.bCPUThread;
		config_cache.bSkipIdle = StartUp.bSkipIdle;
		config_cache.bEnableFPRF = StartUp.bEnableFPRF;
		config_cache.bMMU = StartUp.bMMU;
		config_cache.bMMUBAT = StartUp.bMMUBAT;
		config_cache.iTLBHack = StartUp.iTLBHack;
		config_cache.bVBeam = StartUp.bVBeam;
		config_cache.bFastDiscSpeed = StartUp.bFastDiscSpeed;
		config_cache.bMergeBlocks = StartUp.bMergeBlocks;
		config_cache.bDSPHLE = StartUp.bDSPHLE;
		config_cache.bDisableWiimoteSpeaker = StartUp.bDisableWiimoteSpeaker;
		config_cache.strBackend = StartUp.m_strVideoBackend;

		// General settings
		game_ini.Get("Core", "CPUThread",			&StartUp.bCPUThread, StartUp.bCPUThread);
		game_ini.Get("Core", "SkipIdle",			&StartUp.bSkipIdle, StartUp.bSkipIdle);
		game_ini.Get("Core", "EnableFPRF",			&StartUp.bEnableFPRF, StartUp.bEnableFPRF);
		game_ini.Get("Core", "MMU",					&StartUp.bMMU, StartUp.bMMU);
		game_ini.Get("Core", "BAT",					&StartUp.bMMUBAT, StartUp.bMMUBAT);
		game_ini.Get("Core", "TLBHack",				&StartUp.iTLBHack, StartUp.iTLBHack);
		game_ini.Get("Core", "VBeam",				&StartUp.bVBeam, StartUp.bVBeam);
		game_ini.Get("Core", "FastDiscSpeed",		&StartUp.bFastDiscSpeed, StartUp.bFastDiscSpeed);
		game_ini.Get("Core", "BlockMerging",		&StartUp.bMergeBlocks, StartUp.bMergeBlocks);
		game_ini.Get("Core", "DSPHLE",				&StartUp.bDSPHLE, StartUp.bDSPHLE);
		game_ini.Get("Wii", "DisableWiimoteSpeaker",&StartUp.bDisableWiimoteSpeaker, StartUp.bDisableWiimoteSpeaker);
		game_ini.Get("Core", "GFXBackend", &StartUp.m_strVideoBackend, StartUp.m_strVideoBackend.c_str());
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);

		if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
		{
			StartUp.bCPUThread = Movie::IsDualCore();
			StartUp.bSkipIdle = Movie::IsSkipIdle();
			StartUp.bDSPHLE = Movie::IsDSPHLE();
			StartUp.bProgressive = Movie::IsProgressive();
			StartUp.bFastDiscSpeed = Movie::IsFastDiscSpeed();
			if (Movie::IsUsingMemcard() && Movie::IsStartingFromClearSave() && !StartUp.bWii)
			{
				if (File::Exists("Movie.raw"))
					File::Delete("Movie.raw");
			}
		}
		// Wii settings
		if (StartUp.bWii)
		{
			// Flush possible changes to SYSCONF to file
			SConfig::GetInstance().m_SYSCONF->Save();
		}
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

	if (config_cache.valid)
	{
		config_cache.valid = false;
		StartUp.bCPUThread = config_cache.bCPUThread;
		StartUp.bSkipIdle = config_cache.bSkipIdle;
		StartUp.bEnableFPRF = config_cache.bEnableFPRF;
		StartUp.bMMU = config_cache.bMMU;
		StartUp.bMMUBAT = config_cache.bMMUBAT;
		StartUp.iTLBHack = config_cache.iTLBHack;
		StartUp.bVBeam = config_cache.bVBeam;
		StartUp.bFastDiscSpeed = config_cache.bFastDiscSpeed;
		StartUp.bMergeBlocks = config_cache.bMergeBlocks;
		StartUp.bDSPHLE = config_cache.bDSPHLE;
		StartUp.bDisableWiimoteSpeaker = config_cache.bDisableWiimoteSpeaker;
		StartUp.m_strVideoBackend = config_cache.strBackend;
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);
	}
}

} // namespace
