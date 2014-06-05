// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/HW/SI.h"
#include "DiscIO/NANDContentLoader.h"

SConfig* SConfig::m_Instance;

static const struct
{
	const char* IniText;
	const int   DefaultKey;
	const int   DefaultModifier;
} g_HKData[] = {
#ifdef __APPLE__
	{ "Open",                79 /* 'O' */,        2 /* wxMOD_CMD */ },
	{ "ChangeDisc",          0,                   0 /* wxMOD_NONE */ },
	{ "RefreshList",         0,                   0 /* wxMOD_NONE */ },

	{ "PlayPause",           80 /* 'P' */,        2 /* wxMOD_CMD */ },
	{ "Stop",                87 /* 'W' */,        2 /* wxMOD_CMD */ },
	{ "Reset",               0,                   0 /* wxMOD_NONE */ },
	{ "FrameAdvance",        0,                   0 /* wxMOD_NONE */ },

	{ "StartRecording",      0,                   0 /* wxMOD_NONE */ },
	{ "PlayRecording",       0,                   0 /* wxMOD_NONE */ },
	{ "ExportRecording",     0,                   0 /* wxMOD_NONE */ },
	{ "Readonlymode",        0,                   0 /* wxMOD_NONE */ },

	{ "ToggleFullscreen",    70 /* 'F' */,        2 /* wxMOD_CMD */ },
	{ "Screenshot",          83 /* 'S' */,        2 /* wxMOD_CMD */ },
	{ "Exit",                0,                   0 /* wxMOD_NONE */ },

	{ "Wiimote1Connect",     49 /* '1' */,        2 /* wxMOD_CMD */ },
	{ "Wiimote2Connect",     50 /* '2' */,        2 /* wxMOD_CMD */ },
	{ "Wiimote3Connect",     51 /* '3' */,        2 /* wxMOD_CMD */ },
	{ "Wiimote4Connect",     52 /* '4' */,        2 /* wxMOD_CMD */ },
	{ "BalanceBoardConnect", 53 /* '4' */,        2 /* wxMOD_CMD */ },
#else
	{ "Open",                79 /* 'O' */,        2 /* wxMOD_CONTROL */},
	{ "ChangeDisc",          0,                   0 /* wxMOD_NONE */ },
	{ "RefreshList",         0,                   0 /* wxMOD_NONE */ },

	{ "PlayPause",           349 /* WXK_F10 */,   0 /* wxMOD_NONE */ },
	{ "Stop",                27 /* WXK_ESCAPE */, 0 /* wxMOD_NONE */ },
	{ "Reset",               0,                   0 /* wxMOD_NONE */ },
	{ "FrameAdvance",        0,                   0 /* wxMOD_NONE */ },

	{ "StartRecording",      0,                   0 /* wxMOD_NONE */ },
	{ "PlayRecording",       0,                   0 /* wxMOD_NONE */ },
	{ "ExportRecording",     0,                   0 /* wxMOD_NONE */ },
	{ "Readonlymode",        0,                   0 /* wxMOD_NONE */ },

	{ "ToggleFullscreen",   13 /* WXK_RETURN */,  1 /* wxMOD_ALT */ },
	{ "Screenshot",         348 /* WXK_F9 */,     0 /* wxMOD_NONE */ },
	{ "Exit",                0,                   0 /* wxMOD_NONE */ },

	{ "Wiimote1Connect",    344 /* WXK_F5 */,     1 /* wxMOD_ALT */ },
	{ "Wiimote2Connect",    345 /* WXK_F6 */,     1 /* wxMOD_ALT */ },
	{ "Wiimote3Connect",    346 /* WXK_F7 */,     1 /* wxMOD_ALT */ },
	{ "Wiimote4Connect",    347 /* WXK_F8 */,     1 /* wxMOD_ALT */ },
	{ "BalanceBoardConnect",348 /* WXK_F9 */,     1 /* wxMOD_ALT */ },
#endif
	{ "ToggleIR",            0,                   0 /* wxMOD_NONE */ },
	{ "ToggleAspectRatio",   0,                   0 /* wxMOD_NONE */ },
	{ "ToggleEFBCopies",     0,                   0 /* wxMOD_NONE */ },
	{ "ToggleFog",           0,                   0 /* wxMOD_NONE */ },
	{ "ToggleThrottle",      9 /* '\t' */,        0 /* wxMOD_NONE */ },
	{ "IncreaseFrameLimit",  0,                   0 /* wxMOD_NONE */ },
	{ "DecreaseFrameLimit",  0,                   0 /* wxMOD_NONE */ },
	{ "LoadStateSlot1",      340 /* WXK_F1 */,    0 /* wxMOD_NONE */ },
	{ "LoadStateSlot2",      341 /* WXK_F2 */,    0 /* wxMOD_NONE */ },
	{ "LoadStateSlot3",      342 /* WXK_F3 */,    0 /* wxMOD_NONE */ },
	{ "LoadStateSlot4",      343 /* WXK_F4 */,    0 /* wxMOD_NONE */ },
	{ "LoadStateSlot5",      344 /* WXK_F5 */,    0 /* wxMOD_NONE */ },
	{ "LoadStateSlot6",      345 /* WXK_F6 */,    0 /* wxMOD_NONE */ },
	{ "LoadStateSlot7",      346 /* WXK_F7 */,    0 /* wxMOD_NONE */ },
	{ "LoadStateSlot8",      347 /* WXK_F8 */,    0 /* wxMOD_NONE */ },
	{ "LoadStateSlot9",      0,                   0 /* wxMOD_NONE */ },
	{ "LoadStateSlot10",     0,                   0 /* wxMOD_NONE */ },

	{ "SaveStateSlot1",      340 /* WXK_F1 */,    4 /* wxMOD_SHIFT */ },
	{ "SaveStateSlot2",      341 /* WXK_F2 */,    4 /* wxMOD_SHIFT */ },
	{ "SaveStateSlot3",      342 /* WXK_F3 */,    4 /* wxMOD_SHIFT */ },
	{ "SaveStateSlot4",      343 /* WXK_F4 */,    4 /* wxMOD_SHIFT */ },
	{ "SaveStateSlot5",      344 /* WXK_F5 */,    4 /* wxMOD_SHIFT */ },
	{ "SaveStateSlot6",      345 /* WXK_F6 */,    4 /* wxMOD_SHIFT */ },
	{ "SaveStateSlot7",      346 /* WXK_F7 */,    4 /* wxMOD_SHIFT */ },
	{ "SaveStateSlot8",      347 /* WXK_F8 */,    4 /* wxMOD_SHIFT */ },
	{ "SaveStateSlot9",      0,                   0 /* wxMOD_NONE */ },
	{ "SaveStateSlot10",     0,                   0 /* wxMOD_NONE */ },

	{ "LoadLastState1",      0,                   0 /* wxMOD_NONE */ },
	{ "LoadLastState2",      0,                   0 /* wxMOD_NONE */ },
	{ "LoadLastState3",      0,                   0 /* wxMOD_NONE */ },
	{ "LoadLastState4",      0,                   0 /* wxMOD_NONE */ },
	{ "LoadLastState5",      0,                   0 /* wxMOD_NONE */ },
	{ "LoadLastState6",      0,                   0 /* wxMOD_NONE */ },
	{ "LoadLastState7",      0,                   0 /* wxMOD_NONE */ },
	{ "LoadLastState8",      0,                   0 /* wxMOD_NONE */ },

	{ "SaveFirstState",      0,                   0 /* wxMOD_NONE */ },
	{ "UndoLoadState",       351 /* WXK_F12 */,   0 /* wxMOD_NONE */ },
	{ "UndoSaveState",       351 /* WXK_F12 */,   4 /* wxMOD_SHIFT */ },
	{ "SaveStateFile",       0,                   0 /* wxMOD_NONE */ },
	{ "LoadStateFile",       0,                   0 /* wxMOD_NONE */ },
};

SConfig::SConfig()
{
	// Make sure we have log manager
	LoadSettings();
}

void SConfig::Init()
{
	m_Instance = new SConfig;
}

void SConfig::Shutdown()
{
	delete m_Instance;
	m_Instance = nullptr;
}

SConfig::~SConfig()
{
	SaveSettings();
	delete m_SYSCONF;
}


void SConfig::SaveSettings()
{
	NOTICE_LOG(BOOT, "Saving settings to %s", File::GetUserPath(F_DOLPHINCONFIG_IDX).c_str());
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX)); // load first to not kill unknown stuff

	// General
	ini.Set("General", "LastFilename", m_LastFilename);
	ini.Set("General", "ShowLag", m_ShowLag);

	// ISO folders
	// clear removed folders
	int oldPaths,
		numPaths = (int)m_ISOFolder.size();
	ini.Get("General", "GCMPathes", &oldPaths, 0);
	for (int i = numPaths; i < oldPaths; i++)
	{
		ini.DeleteKey("General", StringFromFormat("GCMPath%i", i));
	}

	ini.Set("General", "GCMPathes", numPaths);

	for (int i = 0; i < numPaths; i++)
	{
		ini.Set("General", StringFromFormat("GCMPath%i", i), m_ISOFolder[i]);
	}

	ini.Set("General", "RecursiveGCMPaths", m_RecursiveISOFolder);
	ini.Set("General", "NANDRootPath",      m_NANDPath);
	ini.Set("General", "WirelessMac",       m_WirelessMac);
	#ifdef USE_GDBSTUB
	ini.Set("General", "GDBPort", m_LocalCoreStartupParameter.iGDBPort);
	#endif

	// Interface
	ini.Set("Interface", "ConfirmStop",         m_LocalCoreStartupParameter.bConfirmStop);
	ini.Set("Interface", "UsePanicHandlers",    m_LocalCoreStartupParameter.bUsePanicHandlers);
	ini.Set("Interface", "OnScreenDisplayMessages", m_LocalCoreStartupParameter.bOnScreenDisplayMessages);
	ini.Set("Interface", "HideCursor",          m_LocalCoreStartupParameter.bHideCursor);
	ini.Set("Interface", "AutoHideCursor",      m_LocalCoreStartupParameter.bAutoHideCursor);
	ini.Set("Interface", "MainWindowPosX",      (m_LocalCoreStartupParameter.iPosX == -32000) ? 0 : m_LocalCoreStartupParameter.iPosX); // TODO - HAX
	ini.Set("Interface", "MainWindowPosY",      (m_LocalCoreStartupParameter.iPosY == -32000) ? 0 : m_LocalCoreStartupParameter.iPosY); // TODO - HAX
	ini.Set("Interface", "MainWindowWidth",     m_LocalCoreStartupParameter.iWidth);
	ini.Set("Interface", "MainWindowHeight",    m_LocalCoreStartupParameter.iHeight);
	ini.Set("Interface", "Language",            m_InterfaceLanguage);
	ini.Set("Interface", "ShowToolbar",         m_InterfaceToolbar);
	ini.Set("Interface", "ShowStatusbar",       m_InterfaceStatusbar);
	ini.Set("Interface", "ShowLogWindow",       m_InterfaceLogWindow);
	ini.Set("Interface", "ShowLogConfigWindow", m_InterfaceLogConfigWindow);
	ini.Set("Interface", "ExtendedFPSInfo",     m_InterfaceExtendedFPSInfo);
	ini.Set("Interface", "ThemeName40",         m_LocalCoreStartupParameter.theme_name);

	// Hotkeys
	for (int i = 0; i < NUM_HOTKEYS; i++)
	{
		ini.Set("Hotkeys", g_HKData[i].IniText, m_LocalCoreStartupParameter.iHotkey[i]);
		ini.Set("Hotkeys", std::string(g_HKData[i].IniText) + "Modifier",
				m_LocalCoreStartupParameter.iHotkeyModifier[i]);
	}

	// Display
	ini.Set("Display", "FullscreenResolution", m_LocalCoreStartupParameter.strFullscreenResolution);
	ini.Set("Display", "Fullscreen",           m_LocalCoreStartupParameter.bFullscreen);
	ini.Set("Display", "RenderToMain",         m_LocalCoreStartupParameter.bRenderToMain);
	ini.Set("Display", "RenderWindowXPos",     m_LocalCoreStartupParameter.iRenderWindowXPos);
	ini.Set("Display", "RenderWindowYPos",     m_LocalCoreStartupParameter.iRenderWindowYPos);
	ini.Set("Display", "RenderWindowWidth",    m_LocalCoreStartupParameter.iRenderWindowWidth);
	ini.Set("Display", "RenderWindowHeight",   m_LocalCoreStartupParameter.iRenderWindowHeight);
	ini.Set("Display", "RenderWindowAutoSize", m_LocalCoreStartupParameter.bRenderWindowAutoSize);
	ini.Set("Display", "KeepWindowOnTop",      m_LocalCoreStartupParameter.bKeepWindowOnTop);
	ini.Set("Display", "ProgressiveScan",      m_LocalCoreStartupParameter.bProgressive);
	ini.Set("Display", "DisableScreenSaver",   m_LocalCoreStartupParameter.bDisableScreenSaver);
	ini.Set("Display", "ForceNTSCJ",           m_LocalCoreStartupParameter.bForceNTSCJ);

	// Game List Control
	ini.Set("GameList", "ListDrives",   m_ListDrives);
	ini.Set("GameList", "ListWad",      m_ListWad);
	ini.Set("GameList", "ListWii",      m_ListWii);
	ini.Set("GameList", "ListGC",       m_ListGC);
	ini.Set("GameList", "ListJap",      m_ListJap);
	ini.Set("GameList", "ListPal",      m_ListPal);
	ini.Set("GameList", "ListUsa",      m_ListUsa);
	ini.Set("GameList", "ListFrance",   m_ListFrance);
	ini.Set("GameList", "ListItaly",    m_ListItaly);
	ini.Set("GameList", "ListKorea",    m_ListKorea);
	ini.Set("GameList", "ListTaiwan",   m_ListTaiwan);
	ini.Set("GameList", "ListUnknown",  m_ListUnknown);
	ini.Set("GameList", "ListSort",     m_ListSort);
	ini.Set("GameList", "ListSortSecondary", m_ListSort2);

	ini.Set("GameList", "ColorCompressed", m_ColorCompressed);

	ini.Set("GameList", "ColumnPlatform", m_showSystemColumn);
	ini.Set("GameList", "ColumnBanner", m_showBannerColumn);
	ini.Set("GameList", "ColumnNotes", m_showNotesColumn);
	ini.Set("GameList", "ColumnID", m_showIDColumn);
	ini.Set("GameList", "ColumnRegion", m_showRegionColumn);
	ini.Set("GameList", "ColumnSize", m_showSizeColumn);
	ini.Set("GameList", "ColumnState", m_showStateColumn);

	// Core
	ini.Set("Core", "HLE_BS2",          m_LocalCoreStartupParameter.bHLE_BS2);
	ini.Set("Core", "CPUCore",          m_LocalCoreStartupParameter.iCPUCore);
	ini.Set("Core", "Fastmem",          m_LocalCoreStartupParameter.bFastmem);
	ini.Set("Core", "CPUThread",        m_LocalCoreStartupParameter.bCPUThread);
	ini.Set("Core", "DSPThread",        m_LocalCoreStartupParameter.bDSPThread);
	ini.Set("Core", "DSPHLE",           m_LocalCoreStartupParameter.bDSPHLE);
	ini.Set("Core", "SkipIdle",         m_LocalCoreStartupParameter.bSkipIdle);
	ini.Set("Core", "DefaultGCM",       m_LocalCoreStartupParameter.m_strDefaultGCM);
	ini.Set("Core", "DVDRoot",          m_LocalCoreStartupParameter.m_strDVDRoot);
	ini.Set("Core", "Apploader",        m_LocalCoreStartupParameter.m_strApploader);
	ini.Set("Core", "EnableCheats",     m_LocalCoreStartupParameter.bEnableCheats);
	ini.Set("Core", "SelectedLanguage", m_LocalCoreStartupParameter.SelectedLanguage);
	ini.Set("Core", "DPL2Decoder",      m_LocalCoreStartupParameter.bDPL2Decoder);
	ini.Set("Core", "Latency",          m_LocalCoreStartupParameter.iLatency);
	ini.Set("Core", "MemcardAPath",     m_strMemoryCardA);
	ini.Set("Core", "MemcardBPath",     m_strMemoryCardB);
	ini.Set("Core", "SlotA",            m_EXIDevice[0]);
	ini.Set("Core", "SlotB",            m_EXIDevice[1]);
	ini.Set("Core", "SerialPort1",      m_EXIDevice[2]);
	ini.Set("Core", "BBA_MAC",          m_bba_mac);
	for (int i = 0; i < MAX_SI_CHANNELS; ++i)
	{
		ini.Set("Core", StringFromFormat("SIDevice%i", i), m_SIDevice[i]);
	}
	ini.Set("Core", "WiiSDCard", m_WiiSDCard);
	ini.Set("Core", "WiiKeyboard", m_WiiKeyboard);
	ini.Set("Core", "WiimoteContinuousScanning", m_WiimoteContinuousScanning);
	ini.Set("Core", "WiimoteEnableSpeaker", m_WiimoteEnableSpeaker);
	ini.Set("Core", "RunCompareServer", m_LocalCoreStartupParameter.bRunCompareServer);
	ini.Set("Core", "RunCompareClient", m_LocalCoreStartupParameter.bRunCompareClient);
	ini.Set("Core", "FrameLimit",       m_Framelimit);
	ini.Set("Core", "FrameSkip",        m_FrameSkip);

	// GFX Backend
	ini.Set("Core", "GFXBackend", m_LocalCoreStartupParameter.m_strVideoBackend);

	// Movie
	ini.Set("Movie", "PauseMovie", m_PauseMovie);
	ini.Set("Movie", "Author", m_strMovieAuthor);

	// DSP
	ini.Set("DSP", "EnableJIT", m_DSPEnableJIT);
	ini.Set("DSP", "DumpAudio", m_DumpAudio);
	ini.Set("DSP", "Backend", sBackend);
	ini.Set("DSP", "Volume", m_Volume);

	// Fifo Player
	ini.Set("FifoPlayer", "LoopReplay", m_LocalCoreStartupParameter.bLoopFifoReplay);

	ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	m_SYSCONF->Save();
}


void SConfig::LoadSettings()
{
	INFO_LOG(BOOT, "Loading Settings from %s", File::GetUserPath(F_DOLPHINCONFIG_IDX).c_str());
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));

	// General
	{
		ini.Get("General", "LastFilename", &m_LastFilename);
		ini.Get("General", "ShowLag", &m_ShowLag, false);
		#ifdef USE_GDBSTUB
		ini.Get("General", "GDBPort", &(m_LocalCoreStartupParameter.iGDBPort), -1);
		#endif

		m_ISOFolder.clear();
		int numGCMPaths;

		if (ini.Get("General", "GCMPathes", &numGCMPaths, 0))
		{
			for (int i = 0; i < numGCMPaths; i++)
			{
				std::string tmpPath;
				ini.Get("General", StringFromFormat("GCMPath%i", i), &tmpPath, "");
				m_ISOFolder.push_back(std::move(tmpPath));
			}
		}

		ini.Get("General", "RecursiveGCMPaths", &m_RecursiveISOFolder, false);

		ini.Get("General", "NANDRootPath", &m_NANDPath);
		m_NANDPath = File::GetUserPath(D_WIIROOT_IDX, m_NANDPath);
		DiscIO::cUIDsys::AccessInstance().UpdateLocation();
		DiscIO::CSharedContent::AccessInstance().UpdateLocation();
		ini.Get("General", "WirelessMac", &m_WirelessMac);
	}

	{
		// Interface
		ini.Get("Interface", "ConfirmStop",             &m_LocalCoreStartupParameter.bConfirmStop,      true);
		ini.Get("Interface", "UsePanicHandlers",        &m_LocalCoreStartupParameter.bUsePanicHandlers, true);
		ini.Get("Interface", "OnScreenDisplayMessages", &m_LocalCoreStartupParameter.bOnScreenDisplayMessages, true);
		ini.Get("Interface", "HideCursor",              &m_LocalCoreStartupParameter.bHideCursor,       false);
		ini.Get("Interface", "AutoHideCursor",          &m_LocalCoreStartupParameter.bAutoHideCursor,   false);
		ini.Get("Interface", "MainWindowPosX",          &m_LocalCoreStartupParameter.iPosX,             100);
		ini.Get("Interface", "MainWindowPosY",          &m_LocalCoreStartupParameter.iPosY,             100);
		ini.Get("Interface", "MainWindowWidth",         &m_LocalCoreStartupParameter.iWidth,            800);
		ini.Get("Interface", "MainWindowHeight",        &m_LocalCoreStartupParameter.iHeight,           600);
		ini.Get("Interface", "Language",                &m_InterfaceLanguage,                           0);
		ini.Get("Interface", "ShowToolbar",             &m_InterfaceToolbar,                            true);
		ini.Get("Interface", "ShowStatusbar",           &m_InterfaceStatusbar,                          true);
		ini.Get("Interface", "ShowLogWindow",           &m_InterfaceLogWindow,                          false);
		ini.Get("Interface", "ShowLogConfigWindow",     &m_InterfaceLogConfigWindow,                    false);
		ini.Get("Interface", "ExtendedFPSInfo",         &m_InterfaceExtendedFPSInfo,                    false);
		ini.Get("Interface", "ThemeName40",             &m_LocalCoreStartupParameter.theme_name,        "Clean");

		// Hotkeys
		for (int i = 0; i < NUM_HOTKEYS; i++)
		{
			ini.Get("Hotkeys", g_HKData[i].IniText,
			        &m_LocalCoreStartupParameter.iHotkey[i], g_HKData[i].DefaultKey);
			ini.Get("Hotkeys", std::string(g_HKData[i].IniText) + "Modifier",
			        &m_LocalCoreStartupParameter.iHotkeyModifier[i], g_HKData[i].DefaultModifier);
		}

		// Display
		ini.Get("Display", "Fullscreen",           &m_LocalCoreStartupParameter.bFullscreen, false);
		ini.Get("Display", "FullscreenResolution", &m_LocalCoreStartupParameter.strFullscreenResolution, "Auto");
		ini.Get("Display", "RenderToMain",         &m_LocalCoreStartupParameter.bRenderToMain, false);
		ini.Get("Display", "RenderWindowXPos",     &m_LocalCoreStartupParameter.iRenderWindowXPos, -1);
		ini.Get("Display", "RenderWindowYPos",     &m_LocalCoreStartupParameter.iRenderWindowYPos, -1);
		ini.Get("Display", "RenderWindowWidth",    &m_LocalCoreStartupParameter.iRenderWindowWidth, 640);
		ini.Get("Display", "RenderWindowHeight",   &m_LocalCoreStartupParameter.iRenderWindowHeight, 480);
		ini.Get("Display", "RenderWindowAutoSize", &m_LocalCoreStartupParameter.bRenderWindowAutoSize, false);
		ini.Get("Display", "KeepWindowOnTop",      &m_LocalCoreStartupParameter.bKeepWindowOnTop, false);
		ini.Get("Display", "ProgressiveScan",      &m_LocalCoreStartupParameter.bProgressive, false);
		ini.Get("Display", "DisableScreenSaver",   &m_LocalCoreStartupParameter.bDisableScreenSaver, true);
		ini.Get("Display", "ForceNTSCJ",           &m_LocalCoreStartupParameter.bForceNTSCJ, false);

		// Game List Control
		ini.Get("GameList", "ListDrives",       &m_ListDrives,  false);
		ini.Get("GameList", "ListWad",          &m_ListWad,     true);
		ini.Get("GameList", "ListWii",          &m_ListWii,     true);
		ini.Get("GameList", "ListGC",           &m_ListGC,      true);
		ini.Get("GameList", "ListJap",          &m_ListJap,     true);
		ini.Get("GameList", "ListPal",          &m_ListPal,     true);
		ini.Get("GameList", "ListUsa",          &m_ListUsa,     true);

		ini.Get("GameList", "ListFrance",       &m_ListFrance,  true);
		ini.Get("GameList", "ListItaly",        &m_ListItaly,   true);
		ini.Get("GameList", "ListKorea",        &m_ListKorea,   true);
		ini.Get("GameList", "ListTaiwan",       &m_ListTaiwan,  true);
		ini.Get("GameList", "ListUnknown",      &m_ListUnknown, true);
		ini.Get("GameList", "ListSort",         &m_ListSort,       3);
		ini.Get("GameList", "ListSortSecondary",&m_ListSort2,  0);

		// Determines if compressed games display in blue
		ini.Get("GameList", "ColorCompressed", &m_ColorCompressed, true);

		// Gamelist columns toggles
		ini.Get("GameList", "ColumnPlatform",   &m_showSystemColumn,  true);
		ini.Get("GameList", "ColumnBanner",     &m_showBannerColumn,  true);
		ini.Get("GameList", "ColumnNotes",      &m_showNotesColumn,   true);
		ini.Get("GameList", "ColumnID",         &m_showIDColumn,      false);
		ini.Get("GameList", "ColumnRegion",     &m_showRegionColumn,  true);
		ini.Get("GameList", "ColumnSize",       &m_showSizeColumn,    true);
		ini.Get("GameList", "ColumnState",      &m_showStateColumn,   true);

		// Core
		ini.Get("Core", "HLE_BS2",      &m_LocalCoreStartupParameter.bHLE_BS2, false);
#ifdef _M_X86
		ini.Get("Core", "CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, 1);
#elif _M_ARM_32
		ini.Get("Core", "CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, 3);
#else
		ini.Get("Core", "CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, 0);
#endif
		ini.Get("Core", "Fastmem",           &m_LocalCoreStartupParameter.bFastmem,      true);
		ini.Get("Core", "DSPThread",         &m_LocalCoreStartupParameter.bDSPThread,    false);
		ini.Get("Core", "DSPHLE",            &m_LocalCoreStartupParameter.bDSPHLE,       true);
		ini.Get("Core", "CPUThread",         &m_LocalCoreStartupParameter.bCPUThread,    true);
		ini.Get("Core", "SkipIdle",          &m_LocalCoreStartupParameter.bSkipIdle,     true);
		ini.Get("Core", "DefaultGCM",        &m_LocalCoreStartupParameter.m_strDefaultGCM);
		ini.Get("Core", "DVDRoot",           &m_LocalCoreStartupParameter.m_strDVDRoot);
		ini.Get("Core", "Apploader",         &m_LocalCoreStartupParameter.m_strApploader);
		ini.Get("Core", "EnableCheats",      &m_LocalCoreStartupParameter.bEnableCheats, false);
		ini.Get("Core", "SelectedLanguage",  &m_LocalCoreStartupParameter.SelectedLanguage, 0);
		ini.Get("Core", "DPL2Decoder",       &m_LocalCoreStartupParameter.bDPL2Decoder, false);
		ini.Get("Core", "Latency",           &m_LocalCoreStartupParameter.iLatency, 2);
		ini.Get("Core", "MemcardAPath",      &m_strMemoryCardA);
		ini.Get("Core", "MemcardBPath",      &m_strMemoryCardB);
		ini.Get("Core", "SlotA",       (int*)&m_EXIDevice[0], EXIDEVICE_MEMORYCARD);
		ini.Get("Core", "SlotB",       (int*)&m_EXIDevice[1], EXIDEVICE_NONE);
		ini.Get("Core", "SerialPort1", (int*)&m_EXIDevice[2], EXIDEVICE_NONE);
		ini.Get("Core", "BBA_MAC",           &m_bba_mac);
		ini.Get("Core", "TimeProfiling",     &m_LocalCoreStartupParameter.bJITILTimeProfiling, false);
		ini.Get("Core", "OutputIR",          &m_LocalCoreStartupParameter.bJITILOutputIR,      false);
		for (int i = 0; i < MAX_SI_CHANNELS; ++i)
		{
			ini.Get("Core", StringFromFormat("SIDevice%i", i), (u32*)&m_SIDevice[i], (i == 0) ? SIDEVICE_GC_CONTROLLER : SIDEVICE_NONE);
		}
		ini.Get("Core", "WiiSDCard",                 &m_WiiSDCard,                                   false);
		ini.Get("Core", "WiiKeyboard",               &m_WiiKeyboard,                                 false);
		ini.Get("Core", "WiimoteContinuousScanning", &m_WiimoteContinuousScanning,                   false);
		ini.Get("Core", "WiimoteEnableSpeaker",      &m_WiimoteEnableSpeaker,                        true);
		ini.Get("Core", "RunCompareServer",          &m_LocalCoreStartupParameter.bRunCompareServer, false);
		ini.Get("Core", "RunCompareClient",          &m_LocalCoreStartupParameter.bRunCompareClient, false);
		ini.Get("Core", "MMU",                       &m_LocalCoreStartupParameter.bMMU,              false);
		ini.Get("Core", "TLBHack",                   &m_LocalCoreStartupParameter.bTLBHack,          false);
		ini.Get("Core", "BBDumpPort",                &m_LocalCoreStartupParameter.iBBDumpPort,       -1);
		ini.Get("Core", "VBeam",                     &m_LocalCoreStartupParameter.bVBeamSpeedHack,   false);
		ini.Get("Core", "SyncGPU",                   &m_LocalCoreStartupParameter.bSyncGPU,          false);
		ini.Get("Core", "FastDiscSpeed",             &m_LocalCoreStartupParameter.bFastDiscSpeed,    false);
		ini.Get("Core", "DCBZ",                      &m_LocalCoreStartupParameter.bDCBZOFF,          false);
		ini.Get("Core", "FrameLimit",                &m_Framelimit,                                  1); // auto frame limit by default
		ini.Get("Core", "FrameSkip",                 &m_FrameSkip,                                   0);

		// GFX Backend
		ini.Get("Core", "GFXBackend",  &m_LocalCoreStartupParameter.m_strVideoBackend, "");

		// Movie
		ini.Get("Movie", "PauseMovie", &m_PauseMovie, false);
		ini.Get("Movie", "Author", &m_strMovieAuthor, "");

		// DSP
		ini.Get("DSP", "EnableJIT", &m_DSPEnableJIT, true);
		ini.Get("DSP", "DumpAudio", &m_DumpAudio, false);
	#if defined __linux__ && HAVE_ALSA
		ini.Get("DSP", "Backend", &sBackend, BACKEND_ALSA);
	#elif defined __APPLE__
		ini.Get("DSP", "Backend", &sBackend, BACKEND_COREAUDIO);
	#elif defined _WIN32
		ini.Get("DSP", "Backend", &sBackend, BACKEND_XAUDIO2);
	#elif defined ANDROID
		ini.Get("DSP", "Backend", &sBackend, BACKEND_OPENSLES);
	#else
		ini.Get("DSP", "Backend", &sBackend, BACKEND_NULLSOUND);
	#endif
		ini.Get("DSP", "Volume", &m_Volume, 100);

		ini.Get("FifoPlayer", "LoopReplay", &m_LocalCoreStartupParameter.bLoopFifoReplay, true);
	}

	m_SYSCONF = new SysConf();
}
