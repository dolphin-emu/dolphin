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

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Common/SysConf.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/HW/EXI.h"
#include "Core/HW/SI.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "VideoCommon/VideoBackendBase.h"

namespace BootManager
{

// TODO this is an ugly hack which allows us to restore values trampled by per-game settings
// Apply fire liberally
struct ConfigCache
{
	bool valid, bCPUThread, bSkipIdle, bEnableFPRF, bMMU, bDCBZOFF, m_EnableJIT, bDSPThread,
	     bVBeamSpeedHack, bSyncGPU, bFastDiscSpeed, bMergeBlocks, bDSPHLE, bHLE_BS2, bTLBHack, bProgressive;
	int iCPUCore, Volume;
	int iWiimoteSource[MAX_BBMOTES];
	SIDevices Pads[MAX_SI_CHANNELS];
	unsigned int framelimit, frameSkip;
	TEXIDevices m_EXIDevice[MAX_EXI_CHANNELS];
	std::string strBackend, sBackend;
	std::string m_strGPUDeterminismMode;
	bool bSetFramelimit, bSetEXIDevice[MAX_EXI_CHANNELS], bSetVolume, bSetPads[MAX_SI_CHANNELS], bSetWiimoteSource[MAX_BBMOTES], bSetFrameSkip;
};
static ConfigCache config_cache;

static GPUDeterminismMode ParseGPUDeterminismMode(const std::string& mode)
{
	if (mode == "auto")
		return GPU_DETERMINISM_AUTO;
	if (mode == "none")
		return GPU_DETERMINISM_NONE;
	if (mode == "fake-completion")
		return GPU_DETERMINISM_FAKE_COMPLETION;

	NOTICE_LOG(BOOT, "Unknown GPU determinism mode %s", mode.c_str());
	return GPU_DETERMINISM_AUTO;
}

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

	// This is saved seperately from everything because it can be changed in SConfig::AutoSetup()
	config_cache.bHLE_BS2 = StartUp.bHLE_BS2;

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
		config_cache.m_strGPUDeterminismMode = StartUp.m_strGPUDeterminismMode;
		config_cache.m_EnableJIT = SConfig::GetInstance().m_DSPEnableJIT;
		config_cache.bDSPThread = StartUp.bDSPThread;
		config_cache.Volume = SConfig::GetInstance().m_Volume;
		config_cache.sBackend = SConfig::GetInstance().sBackend;
		config_cache.framelimit = SConfig::GetInstance().m_Framelimit;
		config_cache.frameSkip = SConfig::GetInstance().m_FrameSkip;
		config_cache.bProgressive = StartUp.bProgressive;
		for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
		{
			config_cache.iWiimoteSource[i] = g_wiimote_sources[i];
		}
		for (unsigned int i = 0; i < MAX_SI_CHANNELS; ++i)
		{
			config_cache.Pads[i] = SConfig::GetInstance().m_SIDevice[i];
		}
		for (unsigned int i = 0; i < MAX_EXI_CHANNELS; ++i)
		{
			config_cache.m_EXIDevice[i] = SConfig::GetInstance().m_EXIDevice[i];
		}
		std::fill_n(config_cache.bSetWiimoteSource, (int)MAX_BBMOTES, false);
		std::fill_n(config_cache.bSetPads, (int)MAX_SI_CHANNELS, false);
		std::fill_n(config_cache.bSetEXIDevice, (int)MAX_EXI_CHANNELS, false);
		config_cache.bSetFramelimit = false;
		config_cache.bSetFrameSkip = false;

		// General settings
		IniFile::Section* core_section     = game_ini.GetOrCreateSection("Core");
		IniFile::Section* dsp_section      = game_ini.GetOrCreateSection("DSP");
		IniFile::Section* controls_section = game_ini.GetOrCreateSection("Controls");

		core_section->Get("CPUThread",        &StartUp.bCPUThread, StartUp.bCPUThread);
		core_section->Get("SkipIdle",         &StartUp.bSkipIdle, StartUp.bSkipIdle);
		core_section->Get("EnableFPRF",       &StartUp.bEnableFPRF, StartUp.bEnableFPRF);
		core_section->Get("MMU",              &StartUp.bMMU, StartUp.bMMU);
		core_section->Get("TLBHack",          &StartUp.bTLBHack, StartUp.bTLBHack);
		core_section->Get("DCBZ",             &StartUp.bDCBZOFF, StartUp.bDCBZOFF);
		core_section->Get("VBeam",            &StartUp.bVBeamSpeedHack, StartUp.bVBeamSpeedHack);
		core_section->Get("SyncGPU",          &StartUp.bSyncGPU, StartUp.bSyncGPU);
		core_section->Get("FastDiscSpeed",    &StartUp.bFastDiscSpeed, StartUp.bFastDiscSpeed);
		core_section->Get("BlockMerging",     &StartUp.bMergeBlocks, StartUp.bMergeBlocks);
		core_section->Get("DSPHLE",           &StartUp.bDSPHLE, StartUp.bDSPHLE);
		core_section->Get("DSPThread",        &StartUp.bDSPThread, StartUp.bDSPThread);
		core_section->Get("GFXBackend",       &StartUp.m_strVideoBackend, StartUp.m_strVideoBackend);
		core_section->Get("CPUCore",          &StartUp.iCPUCore, StartUp.iCPUCore);
		core_section->Get("HLE_BS2",          &StartUp.bHLE_BS2, StartUp.bHLE_BS2);
		core_section->Get("ProgressiveScan",  &StartUp.bProgressive, StartUp.bProgressive);
		if (core_section->Get("FrameLimit",   &SConfig::GetInstance().m_Framelimit, SConfig::GetInstance().m_Framelimit))
			config_cache.bSetFramelimit = true;
		if (core_section->Get("FrameSkip",    &SConfig::GetInstance().m_FrameSkip))
		{
			config_cache.bSetFrameSkip = true;
			Movie::SetFrameSkipping(SConfig::GetInstance().m_FrameSkip);
		}

		if (dsp_section->Get("Volume",        &SConfig::GetInstance().m_Volume, SConfig::GetInstance().m_Volume))
			config_cache.bSetVolume = true;
		dsp_section->Get("EnableJIT",         &SConfig::GetInstance().m_DSPEnableJIT, SConfig::GetInstance().m_DSPEnableJIT);
		dsp_section->Get("Backend",           &SConfig::GetInstance().sBackend, SConfig::GetInstance().sBackend);
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);
		core_section->Get("GPUDeterminismMode", &StartUp.m_strGPUDeterminismMode, StartUp.m_strGPUDeterminismMode);
		StartUp.m_GPUDeterminismMode = ParseGPUDeterminismMode(StartUp.m_strGPUDeterminismMode);

		for (unsigned int i = 0; i < MAX_SI_CHANNELS; ++i)
		{
			int source;
			controls_section->Get(StringFromFormat("PadType%u", i), &source, -1);
			if (source >= (int) SIDEVICE_NONE && source <= (int) SIDEVICE_AM_BASEBOARD)
			{
				SConfig::GetInstance().m_SIDevice[i] = (SIDevices) source;
				config_cache.bSetPads[i] = true;
			}
		}

		// Wii settings
		if (StartUp.bWii)
		{
			// Flush possible changes to SYSCONF to file
			SConfig::GetInstance().m_SYSCONF->Save();

			int source;
			for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
			{
				controls_section->Get(StringFromFormat("WiimoteSource%u", i), &source, -1);
				if (source != -1 && g_wiimote_sources[i] != (unsigned) source && source >= WIIMOTE_SRC_NONE && source <= WIIMOTE_SRC_HYBRID)
				{
					config_cache.bSetWiimoteSource[i] = true;
					g_wiimote_sources[i] = source;
					WiimoteReal::ChangeWiimoteSource(i, source);
				}
			}
			controls_section->Get("WiimoteSourceBB", &source, -1);
			if (source != -1 && g_wiimote_sources[WIIMOTE_BALANCE_BOARD] != (unsigned) source && (source == WIIMOTE_SRC_NONE || source == WIIMOTE_SRC_REAL))
			{
				config_cache.bSetWiimoteSource[WIIMOTE_BALANCE_BOARD] = true;
				g_wiimote_sources[WIIMOTE_BALANCE_BOARD] = source;
				WiimoteReal::ChangeWiimoteSource(WIIMOTE_BALANCE_BOARD, source);
			}
		}
	}

	// Movie settings
	if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
	{
		StartUp.bCPUThread = Movie::IsDualCore();
		StartUp.bSkipIdle = Movie::IsSkipIdle();
		StartUp.bDSPHLE = Movie::IsDSPHLE();
		StartUp.bProgressive = Movie::IsProgressive();
		StartUp.bFastDiscSpeed = Movie::IsFastDiscSpeed();
		StartUp.iCPUCore = Movie::GetCPUMode();
		StartUp.bSyncGPU = Movie::IsSyncGPU();
		for (int i = 0; i < 2; ++i)
		{
			if (Movie::IsUsingMemcard(i) && Movie::IsStartingFromClearSave() && !StartUp.bWii)
			{
				if (File::Exists(File::GetUserPath(D_GCUSER_IDX) + StringFromFormat("Movie%s.raw", (i == 0) ? "A" : "B")))
					File::Delete(File::GetUserPath(D_GCUSER_IDX) + StringFromFormat("Movie%s.raw", (i == 0) ? "A" : "B"));
			}
		}
	}

	if (NetPlay::IsNetPlayRunning())
	{
		StartUp.bCPUThread = g_NetPlaySettings.m_CPUthread;
		StartUp.bDSPHLE = g_NetPlaySettings.m_DSPHLE;
		StartUp.bEnableMemcardSaving = g_NetPlaySettings.m_WriteToMemcard;
		StartUp.iCPUCore = g_NetPlaySettings.m_CPUcore;
		SConfig::GetInstance().m_DSPEnableJIT = g_NetPlaySettings.m_DSPEnableJIT;
		SConfig::GetInstance().m_EXIDevice[0] = g_NetPlaySettings.m_EXIDevice[0];
		SConfig::GetInstance().m_EXIDevice[1] = g_NetPlaySettings.m_EXIDevice[1];
		config_cache.bSetEXIDevice[0] = true;
		config_cache.bSetEXIDevice[1] = true;
	}

	SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", StartUp.bProgressive);

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
		StartUp.m_strGPUDeterminismMode = config_cache.m_strGPUDeterminismMode;
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);
		StartUp.bHLE_BS2 = config_cache.bHLE_BS2;
		SConfig::GetInstance().sBackend = config_cache.sBackend;
		SConfig::GetInstance().m_DSPEnableJIT = config_cache.m_EnableJIT;
		StartUp.bProgressive = config_cache.bProgressive;
		SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", config_cache.bProgressive);

		// Only change these back if they were actually set by game ini, since they can be changed while a game is running.
		if (config_cache.bSetFramelimit)
			SConfig::GetInstance().m_Framelimit = config_cache.framelimit;
		if (config_cache.bSetFrameSkip)
		{
			SConfig::GetInstance().m_FrameSkip = config_cache.frameSkip;
			Movie::SetFrameSkipping(config_cache.frameSkip);
		}
		if (config_cache.bSetVolume)
			SConfig::GetInstance().m_Volume = config_cache.Volume;

		for (unsigned int i = 0; i < MAX_SI_CHANNELS; ++i)
		{
			if (config_cache.bSetPads[i])
			{
				SConfig::GetInstance().m_SIDevice[i] = config_cache.Pads[i];
			}

		}
		for (unsigned int i = 0; i < MAX_EXI_CHANNELS; ++i)
		{
			if (config_cache.bSetEXIDevice[i])
			{
				SConfig::GetInstance().m_EXIDevice[i] = config_cache.m_EXIDevice[i];
			}
		}
		if (StartUp.bWii)
		{
			for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
			{
				if (config_cache.bSetWiimoteSource[i])
				{
					g_wiimote_sources[i] = config_cache.iWiimoteSource[i];
					WiimoteReal::ChangeWiimoteSource(i, config_cache.iWiimoteSource[i]);
				}

			}
		}

	}
}

} // namespace
