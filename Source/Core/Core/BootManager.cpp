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
public:
	// fill the cache with values from the configuration
	void SaveConfig(const SConfig& config);
	// restore values to the configuration from the cache
	void RestoreConfig(SConfig* config);

	// these store if the relevant setting should be reset back later (true) or if it should be left alone on restore (false)
	bool bSetFramelimit, bSetEXIDevice[MAX_EXI_CHANNELS], bSetVolume, bSetPads[MAX_SI_CHANNELS], bSetWiimoteSource[MAX_BBMOTES], bSetFrameSkip;

private:
	bool valid, bCPUThread, bSkipIdle, bSyncGPUOnSkipIdleHack, bFPRF, bAccurateNaNs, bMMU, bDCBZOFF, m_EnableJIT,
	     bSyncGPU, bFastDiscSpeed, bDSPHLE, bHLE_BS2, bProgressive, bPAL60;
	int iSelectedLanguage;
	int iCPUCore, Volume;
	int iWiimoteSource[MAX_BBMOTES];
	SIDevices Pads[MAX_SI_CHANNELS];
	unsigned int framelimit, frameSkip;
	TEXIDevices m_EXIDevice[MAX_EXI_CHANNELS];
	std::string strBackend, sBackend;
	std::string m_strGPUDeterminismMode;
};

void ConfigCache::SaveConfig(const SConfig& config)
{
	valid = true;

	bCPUThread = config.bCPUThread;
	bSkipIdle = config.bSkipIdle;
	bSyncGPUOnSkipIdleHack = config.bSyncGPUOnSkipIdleHack;
	bFPRF = config.bFPRF;
	bAccurateNaNs = config.bAccurateNaNs;
	bMMU = config.bMMU;
	bDCBZOFF = config.bDCBZOFF;
	m_EnableJIT = config.m_DSPEnableJIT;
	bSyncGPU = config.bSyncGPU;
	bFastDiscSpeed = config.bFastDiscSpeed;
	bDSPHLE = config.bDSPHLE;
	bHLE_BS2 = config.bHLE_BS2;
	bProgressive = config.bProgressive;
	bPAL60 = config.bPAL60;
	iSelectedLanguage = config.SelectedLanguage;
	iCPUCore = config.iCPUCore;
	Volume = config.m_Volume;

	for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
	{
		iWiimoteSource[i] = g_wiimote_sources[i];
	}

	for (unsigned int i = 0; i < MAX_SI_CHANNELS; ++i)
	{
		Pads[i] = config.m_SIDevice[i];
	}

	framelimit = config.m_Framelimit;
	frameSkip = config.m_FrameSkip;

	for (unsigned int i = 0; i < MAX_EXI_CHANNELS; ++i)
	{
		m_EXIDevice[i] = config.m_EXIDevice[i];
	}

	strBackend = config.m_strVideoBackend;
	sBackend = config.sBackend;
	m_strGPUDeterminismMode = config.m_strGPUDeterminismMode;

	bSetFramelimit = false;
	std::fill_n(bSetEXIDevice, (int)MAX_EXI_CHANNELS, false);
	bSetVolume = false;
	std::fill_n(bSetPads, (int)MAX_SI_CHANNELS, false);
	std::fill_n(bSetWiimoteSource, (int)MAX_BBMOTES, false);
	bSetFrameSkip = false;
}

void ConfigCache::RestoreConfig(SConfig* config)
{
	if (!valid)
		return;

	valid = false;

	config->bCPUThread = bCPUThread;
	config->bSkipIdle = bSkipIdle;
	config->bSyncGPUOnSkipIdleHack = bSyncGPUOnSkipIdleHack;
	config->bFPRF = bFPRF;
	config->bAccurateNaNs = bAccurateNaNs;
	config->bMMU = bMMU;
	config->bDCBZOFF = bDCBZOFF;
	config->m_DSPEnableJIT = m_EnableJIT;
	config->bSyncGPU = bSyncGPU;
	config->bFastDiscSpeed = bFastDiscSpeed;
	config->bDSPHLE = bDSPHLE;
	config->bHLE_BS2 = bHLE_BS2;
	config->bProgressive = bProgressive;
	config->m_SYSCONF->SetData("IPL.PGS", bProgressive);
	config->bPAL60 = bPAL60;
	config->m_SYSCONF->SetData("IPL.E60", bPAL60);
	config->SelectedLanguage = iSelectedLanguage;
	config->iCPUCore = iCPUCore;

	// Only change these back if they were actually set by game ini, since they can be changed while a game is running.
	if (bSetVolume)
		config->m_Volume = Volume;

	if (config->bWii)
	{
		for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
		{
			if (bSetWiimoteSource[i])
			{
				g_wiimote_sources[i] = iWiimoteSource[i];
				WiimoteReal::ChangeWiimoteSource(i, iWiimoteSource[i]);
			}
		}
	}

	for (unsigned int i = 0; i < MAX_SI_CHANNELS; ++i)
	{
		if (bSetPads[i])
			config->m_SIDevice[i] = Pads[i];
	}

	if (bSetFramelimit)
		config->m_Framelimit = framelimit;

	if (bSetFrameSkip)
	{
		config->m_FrameSkip = frameSkip;
		Movie::SetFrameSkipping(frameSkip);
	}

	for (unsigned int i = 0; i < MAX_EXI_CHANNELS; ++i)
	{
		if (bSetEXIDevice[i])
			config->m_EXIDevice[i] = m_EXIDevice[i];
	}

	config->m_strVideoBackend = strBackend;
	config->sBackend = sBackend;
	config->m_strGPUDeterminismMode = m_strGPUDeterminismMode;
	VideoBackend::ActivateBackend(config->m_strVideoBackend);
}

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

	config_cache.SaveConfig(StartUp);

	// If for example the ISO file is bad we return here
	if (!StartUp.AutoSetup(SConfig::BOOT_DEFAULT))
		return false;

	// Load game specific settings
	{
		IniFile game_ini = StartUp.LoadGameIni();

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
		core_section->Get("PAL60",            &StartUp.bPAL60, StartUp.bPAL60);
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
		StartUp.bPAL60 = Movie::IsPAL60();
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
		StartUp.bEnableMemcardSdWriting = g_NetPlaySettings.m_WriteToMemcard;
		StartUp.iCPUCore = g_NetPlaySettings.m_CPUcore;
		StartUp.SelectedLanguage = g_NetPlaySettings.m_SelectedLanguage;
		StartUp.bOverrideGCLanguage = g_NetPlaySettings.m_OverrideGCLanguage;
		StartUp.bProgressive = g_NetPlaySettings.m_ProgressiveScan;
		StartUp.bPAL60 = g_NetPlaySettings.m_PAL60;
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

	// Apply overrides
	// Some NTSC GameCube games such as Baten Kaitos react strangely to language settings that would be invalid on an NTSC system
	if (!StartUp.bOverrideGCLanguage && StartUp.bNTSC)
	{
		StartUp.SelectedLanguage = 0;
	}

	// Some NTSC Wii games such as Doc Louis's Punch-Out!! and 1942 (Virtual Console) crash if the PAL60 option is enabled
	if (StartUp.bWii && StartUp.bNTSC)
	{
		StartUp.bPAL60 = false;
	}

	SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", StartUp.bProgressive);
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.E60", StartUp.bPAL60);

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
	config_cache.RestoreConfig(&StartUp);
}

} // namespace
