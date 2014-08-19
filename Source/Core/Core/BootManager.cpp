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
	bool valid, m_CPU_thread, m_skip_idle, m_enable_FPRF, m_MMU, m_DCBZOFF, m_EnableJIT, m_DSP_thread,
	     m_vbeam_speed_hack, m_sync_GPU, m_fast_disc_speed, m_merge_blocks, m_DSPHLE, m_HLE_BS2, m_TLB_hack, m_progressive;
	int m_CPU_core, Volume;
	int iWiimoteSource[MAX_BBMOTES];
	SIDevices Pads[MAX_SI_CHANNELS];
	unsigned int framelimit, frameSkip;
	TEXIDevices m_EXIDevice[MAX_EXI_CHANNELS];
	std::string strBackend, sBackend;
	bool bSetFramelimit, bSetEXIDevice[MAX_EXI_CHANNELS], bSetVolume, bSetPads[MAX_SI_CHANNELS], bSetWiimoteSource[MAX_BBMOTES], bSetFrameSkip;
};
static ConfigCache config_cache;

// Boot the ISO or file
bool BootCore(const std::string& _rFilename)
{
	CoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	// Use custom settings for debugging mode
	Host_SetStartupDebuggingParameters();

	StartUp.m_boot_type = CoreStartupParameter::BOOT_ISO;
	StartUp.m_filename = _rFilename;
	SConfig::GetInstance().m_LastFilename = _rFilename;
	SConfig::GetInstance().SaveSettings();
	StartUp.m_run_compare_client = false;
	StartUp.m_run_compare_server = false;

	// This is saved seperately from everything because it can be changed in SConfig::AutoSetup()
	config_cache.m_HLE_BS2 = StartUp.m_HLE_BS2;

	// If for example the ISO file is bad we return here
	if (!StartUp.AutoSetup(CoreStartupParameter::BOOT_DEFAULT))
		return false;

	// Load game specific settings
	std::string unique_id = StartUp.GetUniqueID();
	std::string revision_specific = StartUp.m_revision_specific_unique_ID;
	StartUp.m_game_ini_default = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + unique_id + ".ini";
	if (revision_specific != "")
		StartUp.m_game_ini_default_revision_specific = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + revision_specific + ".ini";
	else
		StartUp.m_game_ini_default_revision_specific = "";
	StartUp.m_game_ini_local = File::GetUserPath(D_GAMESETTINGS_IDX) + unique_id + ".ini";

	if (unique_id.size() == 6)
	{
		IniFile game_ini = StartUp.LoadGameIni();

		config_cache.valid = true;
		config_cache.m_CPU_thread = StartUp.m_CPU_thread;
		config_cache.m_skip_idle = StartUp.m_skip_idle;
		config_cache.m_CPU_core = StartUp.m_CPU_core;
		config_cache.m_enable_FPRF = StartUp.m_enable_FPRF;
		config_cache.m_MMU = StartUp.m_MMU;
		config_cache.m_DCBZOFF = StartUp.m_DCBZOFF;
		config_cache.m_TLB_hack = StartUp.m_TLB_hack;
		config_cache.m_vbeam_speed_hack = StartUp.m_vbeam_speed_hack;
		config_cache.m_sync_GPU = StartUp.m_sync_GPU;
		config_cache.m_fast_disc_speed = StartUp.m_fast_disc_speed;
		config_cache.m_merge_blocks = StartUp.m_merge_blocks;
		config_cache.m_DSPHLE = StartUp.m_DSPHLE;
		config_cache.strBackend = StartUp.m_video_backend;
		config_cache.m_EnableJIT = SConfig::GetInstance().m_DSPEnableJIT;
		config_cache.m_DSP_thread = StartUp.m_DSP_thread;
		config_cache.Volume = SConfig::GetInstance().m_Volume;
		config_cache.sBackend = SConfig::GetInstance().sBackend;
		config_cache.framelimit = SConfig::GetInstance().m_Framelimit;
		config_cache.frameSkip = SConfig::GetInstance().m_FrameSkip;
		config_cache.m_progressive = StartUp.m_progressive;
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

		core_section->Get("CPUThread",        &StartUp.m_CPU_thread, StartUp.m_CPU_thread);
		core_section->Get("SkipIdle",         &StartUp.m_skip_idle, StartUp.m_skip_idle);
		core_section->Get("EnableFPRF",       &StartUp.m_enable_FPRF, StartUp.m_enable_FPRF);
		core_section->Get("MMU",              &StartUp.m_MMU, StartUp.m_MMU);
		core_section->Get("TLBHack",          &StartUp.m_TLB_hack, StartUp.m_TLB_hack);
		core_section->Get("DCBZ",             &StartUp.m_DCBZOFF, StartUp.m_DCBZOFF);
		core_section->Get("VBeam",            &StartUp.m_vbeam_speed_hack, StartUp.m_vbeam_speed_hack);
		core_section->Get("SyncGPU",          &StartUp.m_sync_GPU, StartUp.m_sync_GPU);
		core_section->Get("FastDiscSpeed",    &StartUp.m_fast_disc_speed, StartUp.m_fast_disc_speed);
		core_section->Get("BlockMerging",     &StartUp.m_merge_blocks, StartUp.m_merge_blocks);
		core_section->Get("DSPHLE",           &StartUp.m_DSPHLE, StartUp.m_DSPHLE);
		core_section->Get("DSPThread",        &StartUp.m_DSP_thread, StartUp.m_DSP_thread);
		core_section->Get("GFXBackend",       &StartUp.m_video_backend, StartUp.m_video_backend);
		core_section->Get("CPUCore",          &StartUp.m_CPU_core, StartUp.m_CPU_core);
		core_section->Get("HLE_BS2",          &StartUp.m_HLE_BS2, StartUp.m_HLE_BS2);
		core_section->Get("ProgressiveScan",  &StartUp.m_progressive, StartUp.m_progressive);
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
		VideoBackend::ActivateBackend(StartUp.m_video_backend);

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
		if (StartUp.m_wii)
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
		StartUp.m_CPU_thread = Movie::IsDualCore();
		StartUp.m_skip_idle = Movie::IsSkipIdle();
		StartUp.m_DSPHLE = Movie::IsDSPHLE();
		StartUp.m_progressive = Movie::IsProgressive();
		StartUp.m_fast_disc_speed = Movie::IsFastDiscSpeed();
		StartUp.m_CPU_core = Movie::GetCPUMode();
		StartUp.m_sync_GPU = Movie::IsSyncGPU();
		for (int i = 0; i < 2; ++i)
		{
			if (Movie::IsUsingMemcard(i) && Movie::IsStartingFromClearSave() && !StartUp.m_wii)
			{
				if (File::Exists(File::GetUserPath(D_GCUSER_IDX) + StringFromFormat("Movie%s.raw", (i == 0) ? "A" : "B")))
					File::Delete(File::GetUserPath(D_GCUSER_IDX) + StringFromFormat("Movie%s.raw", (i == 0) ? "A" : "B"));
			}
		}
	}

	if (NetPlay::IsNetPlayRunning())
	{
		StartUp.m_CPU_thread = g_NetPlaySettings.m_CPUthread;
		StartUp.m_DSPHLE = g_NetPlaySettings.m_DSPHLE;
		StartUp.m_enable_memcard_saving = g_NetPlaySettings.m_WriteToMemcard;
		StartUp.m_CPU_core = g_NetPlaySettings.m_CPUcore;
		SConfig::GetInstance().m_DSPEnableJIT = g_NetPlaySettings.m_DSPEnableJIT;
		SConfig::GetInstance().m_EXIDevice[0] = g_NetPlaySettings.m_EXIDevice[0];
		SConfig::GetInstance().m_EXIDevice[1] = g_NetPlaySettings.m_EXIDevice[1];
		config_cache.bSetEXIDevice[0] = true;
		config_cache.bSetEXIDevice[1] = true;
	}

	SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", StartUp.m_progressive);

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

	CoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	StartUp.m_unique_ID = "00000000";
	if (config_cache.valid)
	{
		config_cache.valid = false;
		StartUp.m_CPU_thread = config_cache.m_CPU_thread;
		StartUp.m_skip_idle = config_cache.m_skip_idle;
		StartUp.m_CPU_core = config_cache.m_CPU_core;
		StartUp.m_enable_FPRF = config_cache.m_enable_FPRF;
		StartUp.m_MMU = config_cache.m_MMU;
		StartUp.m_DCBZOFF = config_cache.m_DCBZOFF;
		StartUp.m_TLB_hack = config_cache.m_TLB_hack;
		StartUp.m_vbeam_speed_hack = config_cache.m_vbeam_speed_hack;
		StartUp.m_sync_GPU = config_cache.m_sync_GPU;
		StartUp.m_fast_disc_speed = config_cache.m_fast_disc_speed;
		StartUp.m_merge_blocks = config_cache.m_merge_blocks;
		StartUp.m_DSPHLE = config_cache.m_DSPHLE;
		StartUp.m_DSP_thread = config_cache.m_DSP_thread;
		StartUp.m_video_backend = config_cache.strBackend;
		VideoBackend::ActivateBackend(StartUp.m_video_backend);
		StartUp.m_HLE_BS2 = config_cache.m_HLE_BS2;
		SConfig::GetInstance().sBackend = config_cache.sBackend;
		SConfig::GetInstance().m_DSPEnableJIT = config_cache.m_EnableJIT;
		StartUp.m_progressive = config_cache.m_progressive;
		SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", config_cache.m_progressive);

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
		if (StartUp.m_wii)
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
