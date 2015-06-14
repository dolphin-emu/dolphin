// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
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
#include "Core/HW/Sram.h"
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
	bool valid, bCPUThread, bSkipIdle, bSyncGPUOnSkipIdleHack, bFPRF, bAccurateNaNs, bMMU, bDCBZOFF, m_EnableJIT, bDSPThread,
	     bSyncGPU, bFastDiscSpeed, bDSPHLE, bHLE_BS2, bProgressive;
	int iSelectedLanguage;
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
	SConfig& StartUp = SConfig::GetInstance();

	// Use custom settings for debugging mode
	Host_SetStartupDebuggingParameters();

	StartUp.m_BootType = SConfig::BOOT_ISO;
	StartUp.m_strFilename = _rFilename;
	SConfig::GetInstance().m_LastFilename = _rFilename;
	SConfig::GetInstance().SaveSettings();
	StartUp.bRunCompareClient = false;
	StartUp.bRunCompareServer = false;

	// This is saved separately from everything because it can be changed in SConfig::AutoSetup()
	config_cache.bHLE_BS2 = StartUp.bHLE_BS2;

	// If for example the ISO file is bad we return here
	if (!StartUp.AutoSetup(SConfig::BOOT_DEFAULT))
		return false;

	// Load game specific settings
	if (StartUp.GetUniqueID().size() == 6)
	{
		IniFile game_ini = StartUp.LoadGameIni();

		config_cache.valid = true;
		config_cache.bCPUThread = StartUp.bCPUThread;
		config_cache.bSkipIdle = StartUp.bSkipIdle;
		config_cache.bSyncGPUOnSkipIdleHack = StartUp.bSyncGPUOnSkipIdleHack;
		config_cache.iCPUCore = StartUp.iCPUCore;
		config_cache.bFPRF = StartUp.bFPRF;
		config_cache.bAccurateNaNs = StartUp.bAccurateNaNs;
		config_cache.bMMU = StartUp.bMMU;
		config_cache.bDCBZOFF = StartUp.bDCBZOFF;
		config_cache.bSyncGPU = StartUp.bSyncGPU;
		config_cache.bFastDiscSpeed = StartUp.bFastDiscSpeed;
		config_cache.bDSPHLE = StartUp.bDSPHLE;
		config_cache.strBackend = StartUp.m_strVideoBackend;
		config_cache.m_strGPUDeterminismMode = StartUp.m_strGPUDeterminismMode;
		config_cache.m_EnableJIT = SConfig::GetInstance().m_DSPEnableJIT;
		config_cache.Volume = SConfig::GetInstance().m_Volume;
		config_cache.sBackend = SConfig::GetInstance().sBackend;
		config_cache.framelimit = SConfig::GetInstance().m_Framelimit;
		config_cache.frameSkip = SConfig::GetInstance().m_FrameSkip;
		config_cache.bProgressive = StartUp.bProgressive;
		config_cache.iSelectedLanguage = StartUp.SelectedLanguage;
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
		core_section->Get("SyncOnSkipIdle",   &StartUp.bSyncGPUOnSkipIdleHack, StartUp.bSyncGPUOnSkipIdleHack);
		core_section->Get("FPRF",             &StartUp.bFPRF, StartUp.bFPRF);
		core_section->Get("AccurateNaNs",     &StartUp.bAccurateNaNs, StartUp.bAccurateNaNs);
		core_section->Get("MMU",              &StartUp.bMMU, StartUp.bMMU);
		core_section->Get("DCBZ",             &StartUp.bDCBZOFF, StartUp.bDCBZOFF);
		core_section->Get("SyncGPU",          &StartUp.bSyncGPU, StartUp.bSyncGPU);
		core_section->Get("FastDiscSpeed",    &StartUp.bFastDiscSpeed, StartUp.bFastDiscSpeed);
		core_section->Get("DSPHLE",           &StartUp.bDSPHLE, StartUp.bDSPHLE);
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

		// Some NTSC GameCube games such as Baten Kaitos react strangely to language settings that would be invalid on an NTSC system
		if (!StartUp.bOverrideGCLanguage && StartUp.bNTSC)
		{
			StartUp.SelectedLanguage = 0;
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

	StartUp.m_GPUDeterminismMode = ParseGPUDeterminismMode(StartUp.m_strGPUDeterminismMode);

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
		StartUp.SelectedLanguage = g_NetPlaySettings.m_SelectedLanguage;
		StartUp.bOverrideGCLanguage = g_NetPlaySettings.m_OverrideGCLanguage;
		SConfig::GetInstance().m_DSPEnableJIT = g_NetPlaySettings.m_DSPEnableJIT;
		SConfig::GetInstance().m_OCEnable = g_NetPlaySettings.m_OCEnable;
		SConfig::GetInstance().m_OCFactor = g_NetPlaySettings.m_OCFactor;
		SConfig::GetInstance().m_EXIDevice[0] = g_NetPlaySettings.m_EXIDevice[0];
		SConfig::GetInstance().m_EXIDevice[1] = g_NetPlaySettings.m_EXIDevice[1];
		config_cache.bSetEXIDevice[0] = true;
		config_cache.bSetEXIDevice[1] = true;
	}
	else
	{
		g_SRAM_netplay_initialized = false;
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

	SConfig& StartUp = SConfig::GetInstance();

	StartUp.m_strUniqueID = "00000000";
	if (config_cache.valid)
	{
		config_cache.valid = false;
		StartUp.bCPUThread = config_cache.bCPUThread;
		StartUp.bSkipIdle = config_cache.bSkipIdle;
		StartUp.bSyncGPUOnSkipIdleHack = config_cache.bSyncGPUOnSkipIdleHack;
		StartUp.iCPUCore = config_cache.iCPUCore;
		StartUp.bFPRF = config_cache.bFPRF;
		StartUp.bAccurateNaNs = config_cache.bAccurateNaNs;
		StartUp.bMMU = config_cache.bMMU;
		StartUp.bDCBZOFF = config_cache.bDCBZOFF;
		StartUp.bSyncGPU = config_cache.bSyncGPU;
		StartUp.bFastDiscSpeed = config_cache.bFastDiscSpeed;
		StartUp.bDSPHLE = config_cache.bDSPHLE;
		StartUp.m_strVideoBackend = config_cache.strBackend;
		StartUp.m_strGPUDeterminismMode = config_cache.m_strGPUDeterminismMode;
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);
		StartUp.bHLE_BS2 = config_cache.bHLE_BS2;
		SConfig::GetInstance().sBackend = config_cache.sBackend;
		SConfig::GetInstance().m_DSPEnableJIT = config_cache.m_EnableJIT;
		StartUp.bProgressive = config_cache.bProgressive;
		StartUp.SelectedLanguage = config_cache.iSelectedLanguage;
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
