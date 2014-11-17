// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
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
	{ "Open",                79 /* 'O' */,        2 /* wxMOD_CONTROL */},
	{ "ChangeDisc",          0,                   0 /* wxMOD_NONE */ },
	{ "RefreshList",         0,                   0 /* wxMOD_NONE */ },
#ifdef __APPLE__
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

	{ "FreelookIncreaseSpeed",49 /* '1' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookDecreaseSpeed",50 /* '2' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookResetSpeed",   70 /* 'F' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookUp",           69 /* 'E' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookDown",         81 /* 'Q' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookLeft",         65 /* 'A' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookRight",        68 /* 'D' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookZoomIn",       87 /* 'W' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookZoomOut",      83 /* 'S' */,       4 /* wxMOD_SHIFT */ },
	{ "FreelookReset",        82 /* 'R' */,       4 /* wxMOD_SHIFT */ },

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

	{ "SelectStateSlot1",	0 ,	0  },
	{ "SelectStateSlot2",	0 ,	0  },
	{ "SelectStateSlot3",	0,	0  },
	{ "SelectStateSlot4",	0 ,	0  },
	{ "SelectStateSlot5",	0 ,	0  },
	{ "SelectStateSlot6",	0 ,	0  },
	{ "SelectStateSlot7",	0 ,	0  },
	{ "SelectStateSlot8",	0 ,	0  },
	{ "SelectStateSlot9",	0 ,	0  },
	{ "SelectStateSlot10",	0 ,	0  },
	{ "SaveSelectedSlot",	0 ,	0  },
	{ "LoadSelectedSlot",	0 ,	0  },

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

	SaveGeneralSettings(ini);
	SaveInterfaceSettings(ini);
	SaveHotkeySettings(ini);
	SaveDisplaySettings(ini);
	SaveGameListSettings(ini);
	SaveCoreSettings(ini);
	SaveMovieSettings(ini);
	SaveDSPSettings(ini);
	SaveInputSettings(ini);
	SaveFifoPlayerSettings(ini);

	ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	m_SYSCONF->Save();
}

void SConfig::SaveGeneralSettings(IniFile& ini)
{
	IniFile::Section* general = ini.GetOrCreateSection("General");

	// General
	general->Set("LastFilename", m_LastFilename);
	general->Set("ShowLag", m_ShowLag);
	general->Set("ShowFrameCount", m_ShowFrameCount);

	// ISO folders
	// Clear removed folders
	int oldPaths;
	int numPaths = (int)m_ISOFolder.size();
	general->Get("ISOPaths", &oldPaths, 0);
	for (int i = numPaths; i < oldPaths; i++)
	{
		ini.DeleteKey("General", StringFromFormat("ISOPath%i", i));
	}

	general->Set("ISOPaths", numPaths);
	for (int i = 0; i < numPaths; i++)
	{
		general->Set(StringFromFormat("ISOPath%i", i), m_ISOFolder[i]);
	}

	general->Set("RecursiveISOPaths", m_RecursiveISOFolder);
	general->Set("NANDRootPath", m_NANDPath);
	general->Set("WirelessMac", m_WirelessMac);

#ifdef USE_GDBSTUB
	general->Set("GDBPort", m_LocalCoreStartupParameter.iGDBPort);
#endif
}

void SConfig::SaveInterfaceSettings(IniFile& ini)
{
	IniFile::Section* interface = ini.GetOrCreateSection("Interface");

	interface->Set("ConfirmStop", m_LocalCoreStartupParameter.bConfirmStop);
	interface->Set("UsePanicHandlers", m_LocalCoreStartupParameter.bUsePanicHandlers);
	interface->Set("OnScreenDisplayMessages", m_LocalCoreStartupParameter.bOnScreenDisplayMessages);
	interface->Set("HideCursor", m_LocalCoreStartupParameter.bHideCursor);
	interface->Set("AutoHideCursor", m_LocalCoreStartupParameter.bAutoHideCursor);
	interface->Set("MainWindowPosX", (m_LocalCoreStartupParameter.iPosX == -32000) ? 0 : m_LocalCoreStartupParameter.iPosX); // TODO - HAX
	interface->Set("MainWindowPosY", (m_LocalCoreStartupParameter.iPosY == -32000) ? 0 : m_LocalCoreStartupParameter.iPosY); // TODO - HAX
	interface->Set("MainWindowWidth", m_LocalCoreStartupParameter.iWidth);
	interface->Set("MainWindowHeight", m_LocalCoreStartupParameter.iHeight);
	interface->Set("Language", m_InterfaceLanguage);
	interface->Set("ShowToolbar", m_InterfaceToolbar);
	interface->Set("ShowStatusbar", m_InterfaceStatusbar);
	interface->Set("ShowLogWindow", m_InterfaceLogWindow);
	interface->Set("ShowLogConfigWindow", m_InterfaceLogConfigWindow);
	interface->Set("ExtendedFPSInfo", m_InterfaceExtendedFPSInfo);
	interface->Set("ThemeName40", m_LocalCoreStartupParameter.theme_name);
}

void SConfig::SaveHotkeySettings(IniFile& ini)
{
	IniFile::Section* hotkeys = ini.GetOrCreateSection("Hotkeys");

	for (int i = 0; i < NUM_HOTKEYS; i++)
	{
		hotkeys->Set(g_HKData[i].IniText, m_LocalCoreStartupParameter.iHotkey[i]);
		hotkeys->Set(std::string(g_HKData[i].IniText) + "Modifier",
			m_LocalCoreStartupParameter.iHotkeyModifier[i]);
	}
}

void SConfig::SaveDisplaySettings(IniFile& ini)
{
	IniFile::Section* display = ini.GetOrCreateSection("Display");

	display->Set("FullscreenResolution", m_LocalCoreStartupParameter.strFullscreenResolution);
	display->Set("Fullscreen", m_LocalCoreStartupParameter.bFullscreen);
	display->Set("RenderToMain", m_LocalCoreStartupParameter.bRenderToMain);
	display->Set("RenderWindowXPos", m_LocalCoreStartupParameter.iRenderWindowXPos);
	display->Set("RenderWindowYPos", m_LocalCoreStartupParameter.iRenderWindowYPos);
	display->Set("RenderWindowWidth", m_LocalCoreStartupParameter.iRenderWindowWidth);
	display->Set("RenderWindowHeight", m_LocalCoreStartupParameter.iRenderWindowHeight);
	display->Set("RenderWindowAutoSize", m_LocalCoreStartupParameter.bRenderWindowAutoSize);
	display->Set("KeepWindowOnTop", m_LocalCoreStartupParameter.bKeepWindowOnTop);
	display->Set("ProgressiveScan", m_LocalCoreStartupParameter.bProgressive);
	display->Set("DisableScreenSaver", m_LocalCoreStartupParameter.bDisableScreenSaver);
	display->Set("ForceNTSCJ", m_LocalCoreStartupParameter.bForceNTSCJ);
}

void SConfig::SaveGameListSettings(IniFile& ini)
{
	IniFile::Section* gamelist = ini.GetOrCreateSection("GameList");

	gamelist->Set("ListDrives", m_ListDrives);
	gamelist->Set("ListWad", m_ListWad);
	gamelist->Set("ListWii", m_ListWii);
	gamelist->Set("ListGC", m_ListGC);
	gamelist->Set("ListJap", m_ListJap);
	gamelist->Set("ListPal", m_ListPal);
	gamelist->Set("ListUsa", m_ListUsa);
	gamelist->Set("ListFrance", m_ListFrance);
	gamelist->Set("ListItaly", m_ListItaly);
	gamelist->Set("ListKorea", m_ListKorea);
	gamelist->Set("ListTaiwan", m_ListTaiwan);
	gamelist->Set("ListUnknown", m_ListUnknown);
	gamelist->Set("ListSort", m_ListSort);
	gamelist->Set("ListSortSecondary", m_ListSort2);

	gamelist->Set("ColorCompressed", m_ColorCompressed);

	gamelist->Set("ColumnPlatform", m_showSystemColumn);
	gamelist->Set("ColumnBanner", m_showBannerColumn);
	gamelist->Set("ColumnNotes", m_showNotesColumn);
	gamelist->Set("ColumnID", m_showIDColumn);
	gamelist->Set("ColumnRegion", m_showRegionColumn);
	gamelist->Set("ColumnSize", m_showSizeColumn);
	gamelist->Set("ColumnState", m_showStateColumn);
}

void SConfig::SaveCoreSettings(IniFile& ini)
{
	IniFile::Section* core = ini.GetOrCreateSection("Core");

	core->Set("HLE_BS2", m_LocalCoreStartupParameter.bHLE_BS2);
	core->Set("CPUCore", m_LocalCoreStartupParameter.iCPUCore);
	core->Set("Fastmem", m_LocalCoreStartupParameter.bFastmem);
	core->Set("CPUThread", m_LocalCoreStartupParameter.bCPUThread);
	core->Set("DSPThread", m_LocalCoreStartupParameter.bDSPThread);
	core->Set("DSPHLE", m_LocalCoreStartupParameter.bDSPHLE);
	core->Set("SkipIdle", m_LocalCoreStartupParameter.bSkipIdle);
	core->Set("DefaultISO", m_LocalCoreStartupParameter.m_strDefaultISO);
	core->Set("DVDRoot", m_LocalCoreStartupParameter.m_strDVDRoot);
	core->Set("Apploader", m_LocalCoreStartupParameter.m_strApploader);
	core->Set("EnableCheats", m_LocalCoreStartupParameter.bEnableCheats);
	core->Set("SelectedLanguage", m_LocalCoreStartupParameter.SelectedLanguage);
	core->Set("DPL2Decoder", m_LocalCoreStartupParameter.bDPL2Decoder);
	core->Set("Latency", m_LocalCoreStartupParameter.iLatency);
	core->Set("MemcardAPath", m_strMemoryCardA);
	core->Set("MemcardBPath", m_strMemoryCardB);
	core->Set("SlotA", m_EXIDevice[0]);
	core->Set("SlotB", m_EXIDevice[1]);
	core->Set("SerialPort1", m_EXIDevice[2]);
	core->Set("BBA_MAC", m_bba_mac);
	for (int i = 0; i < MAX_SI_CHANNELS; ++i)
	{
		core->Set(StringFromFormat("SIDevice%i", i), m_SIDevice[i]);
	}
	core->Set("WiiSDCard", m_WiiSDCard);
	core->Set("WiiKeyboard", m_WiiKeyboard);
	core->Set("WiimoteContinuousScanning", m_WiimoteContinuousScanning);
	core->Set("WiimoteEnableSpeaker", m_WiimoteEnableSpeaker);
	core->Set("RunCompareServer", m_LocalCoreStartupParameter.bRunCompareServer);
	core->Set("RunCompareClient", m_LocalCoreStartupParameter.bRunCompareClient);
	core->Set("FrameLimit", m_Framelimit);
	core->Set("FrameSkip", m_FrameSkip);
	core->Set("GFXBackend", m_LocalCoreStartupParameter.m_strVideoBackend);
	core->Set("GPUDeterminismMode", m_LocalCoreStartupParameter.m_strGPUDeterminismMode);
}

void SConfig::SaveMovieSettings(IniFile& ini)
{
	IniFile::Section* movie = ini.GetOrCreateSection("Movie");

	movie->Set("PauseMovie", m_PauseMovie);
	movie->Set("Author", m_strMovieAuthor);
	movie->Set("DumpFrames", m_DumpFrames);
	movie->Set("ShowInputDisplay", m_ShowInputDisplay);
}

void SConfig::SaveDSPSettings(IniFile& ini)
{
	IniFile::Section* dsp = ini.GetOrCreateSection("DSP");

	dsp->Set("EnableJIT", m_DSPEnableJIT);
	dsp->Set("DumpAudio", m_DumpAudio);
	dsp->Set("Backend", sBackend);
	dsp->Set("Volume", m_Volume);
	dsp->Set("CaptureLog", m_DSPCaptureLog);
}

void SConfig::SaveInputSettings(IniFile& ini)
{
	IniFile::Section* input = ini.GetOrCreateSection("Input");

	input->Set("BackgroundInput", m_BackgroundInput);
}

void SConfig::SaveFifoPlayerSettings(IniFile& ini)
{
	IniFile::Section* fifoplayer = ini.GetOrCreateSection("FifoPlayer");

	fifoplayer->Set("LoopReplay", m_LocalCoreStartupParameter.bLoopFifoReplay);
}

void SConfig::LoadSettings()
{
	INFO_LOG(BOOT, "Loading Settings from %s", File::GetUserPath(F_DOLPHINCONFIG_IDX).c_str());
	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));

	LoadGeneralSettings(ini);
	LoadInterfaceSettings(ini);
	LoadHotkeySettings(ini);
	LoadDisplaySettings(ini);
	LoadGameListSettings(ini);
	LoadCoreSettings(ini);
	LoadMovieSettings(ini);
	LoadDSPSettings(ini);
	LoadInputSettings(ini);
	LoadFifoPlayerSettings(ini);

	m_SYSCONF = new SysConf();
}

void SConfig::LoadGeneralSettings(IniFile& ini)
{
	IniFile::Section* general = ini.GetOrCreateSection("General");

	general->Get("LastFilename", &m_LastFilename);
	general->Get("ShowLag", &m_ShowLag, false);
	general->Get("ShowFrameCount", &m_ShowFrameCount, false);
#ifdef USE_GDBSTUB
	general->Get("GDBPort", &(m_LocalCoreStartupParameter.iGDBPort), -1);
#endif

	m_ISOFolder.clear();
	int numISOPaths;

	if (general->Get("ISOPaths", &numISOPaths, 0))
	{
		for (int i = 0; i < numISOPaths; i++)
		{
			std::string tmpPath;
			general->Get(StringFromFormat("ISOPath%i", i), &tmpPath, "");
			m_ISOFolder.push_back(std::move(tmpPath));
		}
	}

	general->Get("RecursiveISOPaths", &m_RecursiveISOFolder, false);

	general->Get("NANDRootPath", &m_NANDPath);
	m_NANDPath = File::GetUserPath(D_WIIROOT_IDX, m_NANDPath);
	DiscIO::cUIDsys::AccessInstance().UpdateLocation();
	DiscIO::CSharedContent::AccessInstance().UpdateLocation();
	general->Get("WirelessMac", &m_WirelessMac);
}

void SConfig::LoadInterfaceSettings(IniFile& ini)
{
	IniFile::Section* interface = ini.GetOrCreateSection("Interface");

	interface->Get("ConfirmStop",             &m_LocalCoreStartupParameter.bConfirmStop,      true);
	interface->Get("UsePanicHandlers",        &m_LocalCoreStartupParameter.bUsePanicHandlers, true);
	interface->Get("OnScreenDisplayMessages", &m_LocalCoreStartupParameter.bOnScreenDisplayMessages, true);
	interface->Get("HideCursor",              &m_LocalCoreStartupParameter.bHideCursor,       false);
	interface->Get("AutoHideCursor",          &m_LocalCoreStartupParameter.bAutoHideCursor,   false);
	interface->Get("MainWindowPosX",          &m_LocalCoreStartupParameter.iPosX,             100);
	interface->Get("MainWindowPosY",          &m_LocalCoreStartupParameter.iPosY,             100);
	interface->Get("MainWindowWidth",         &m_LocalCoreStartupParameter.iWidth,            800);
	interface->Get("MainWindowHeight",        &m_LocalCoreStartupParameter.iHeight,           600);
	interface->Get("Language",                &m_InterfaceLanguage,                           0);
	interface->Get("ShowToolbar",             &m_InterfaceToolbar,                            true);
	interface->Get("ShowStatusbar",           &m_InterfaceStatusbar,                          true);
	interface->Get("ShowLogWindow",           &m_InterfaceLogWindow,                          false);
	interface->Get("ShowLogConfigWindow",     &m_InterfaceLogConfigWindow,                    false);
	interface->Get("ExtendedFPSInfo",         &m_InterfaceExtendedFPSInfo,                    false);
	interface->Get("ThemeName40",             &m_LocalCoreStartupParameter.theme_name,        "Clean");
}

void SConfig::LoadHotkeySettings(IniFile& ini)
{
	IniFile::Section* hotkeys = ini.GetOrCreateSection("Hotkeys");

	for (int i = 0; i < NUM_HOTKEYS; i++)
	{
		hotkeys->Get(g_HKData[i].IniText,
		    &m_LocalCoreStartupParameter.iHotkey[i], g_HKData[i].DefaultKey);
		hotkeys->Get(std::string(g_HKData[i].IniText) + "Modifier",
		    &m_LocalCoreStartupParameter.iHotkeyModifier[i], g_HKData[i].DefaultModifier);
	}
}

void SConfig::LoadDisplaySettings(IniFile& ini)
{
	IniFile::Section* display = ini.GetOrCreateSection("Display");

	display->Get("Fullscreen",           &m_LocalCoreStartupParameter.bFullscreen,             false);
	display->Get("FullscreenResolution", &m_LocalCoreStartupParameter.strFullscreenResolution, "Auto");
	display->Get("RenderToMain",         &m_LocalCoreStartupParameter.bRenderToMain,           false);
	display->Get("RenderWindowXPos",     &m_LocalCoreStartupParameter.iRenderWindowXPos,       -1);
	display->Get("RenderWindowYPos",     &m_LocalCoreStartupParameter.iRenderWindowYPos,       -1);
	display->Get("RenderWindowWidth",    &m_LocalCoreStartupParameter.iRenderWindowWidth,      640);
	display->Get("RenderWindowHeight",   &m_LocalCoreStartupParameter.iRenderWindowHeight,     480);
	display->Get("RenderWindowAutoSize", &m_LocalCoreStartupParameter.bRenderWindowAutoSize,   false);
	display->Get("KeepWindowOnTop",      &m_LocalCoreStartupParameter.bKeepWindowOnTop,        false);
	display->Get("ProgressiveScan",      &m_LocalCoreStartupParameter.bProgressive,            false);
	display->Get("DisableScreenSaver",   &m_LocalCoreStartupParameter.bDisableScreenSaver,     true);
	display->Get("ForceNTSCJ",           &m_LocalCoreStartupParameter.bForceNTSCJ,             false);
}

void SConfig::LoadGameListSettings(IniFile& ini)
{
	IniFile::Section* gamelist = ini.GetOrCreateSection("GameList");

	gamelist->Get("ListDrives",       &m_ListDrives,  false);
	gamelist->Get("ListWad",          &m_ListWad,     true);
	gamelist->Get("ListWii",          &m_ListWii,     true);
	gamelist->Get("ListGC",           &m_ListGC,      true);
	gamelist->Get("ListJap",          &m_ListJap,     true);
	gamelist->Get("ListPal",          &m_ListPal,     true);
	gamelist->Get("ListUsa",          &m_ListUsa,     true);

	gamelist->Get("ListFrance",       &m_ListFrance,  true);
	gamelist->Get("ListItaly",        &m_ListItaly,   true);
	gamelist->Get("ListKorea",        &m_ListKorea,   true);
	gamelist->Get("ListTaiwan",       &m_ListTaiwan,  true);
	gamelist->Get("ListUnknown",      &m_ListUnknown, true);
	gamelist->Get("ListSort",         &m_ListSort,       3);
	gamelist->Get("ListSortSecondary",&m_ListSort2,  0);

	// Determines if compressed games display in blue
	gamelist->Get("ColorCompressed", &m_ColorCompressed, true);

	// Gamelist columns toggles
	gamelist->Get("ColumnPlatform",   &m_showSystemColumn,  true);
	gamelist->Get("ColumnBanner",     &m_showBannerColumn,  true);
	gamelist->Get("ColumnNotes",      &m_showNotesColumn,   true);
	gamelist->Get("ColumnID",         &m_showIDColumn,      false);
	gamelist->Get("ColumnRegion",     &m_showRegionColumn,  true);
	gamelist->Get("ColumnSize",       &m_showSizeColumn,    true);
	gamelist->Get("ColumnState",      &m_showStateColumn,   true);
}

void SConfig::LoadCoreSettings(IniFile& ini)
{
	IniFile::Section* core = ini.GetOrCreateSection("Core");

	core->Get("HLE_BS2",      &m_LocalCoreStartupParameter.bHLE_BS2, false);
#ifdef _M_X86
	core->Get("CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, SCoreStartupParameter::CORE_JIT64);
#elif _M_ARM_32
	core->Get("CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, SCoreStartupParameter::CORE_JITARM);
#else
	core->Get("CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, SCoreStartupParameter::CORE_INTERPRETER);
#endif
	core->Get("Fastmem",           &m_LocalCoreStartupParameter.bFastmem,      true);
	core->Get("DSPThread",         &m_LocalCoreStartupParameter.bDSPThread,    false);
	core->Get("DSPHLE",            &m_LocalCoreStartupParameter.bDSPHLE,       true);
	core->Get("CPUThread",         &m_LocalCoreStartupParameter.bCPUThread,    true);
	core->Get("SkipIdle",          &m_LocalCoreStartupParameter.bSkipIdle,     true);
	core->Get("DefaultISO",        &m_LocalCoreStartupParameter.m_strDefaultISO);
	core->Get("DVDRoot",           &m_LocalCoreStartupParameter.m_strDVDRoot);
	core->Get("Apploader",         &m_LocalCoreStartupParameter.m_strApploader);
	core->Get("EnableCheats",      &m_LocalCoreStartupParameter.bEnableCheats, false);
	core->Get("SelectedLanguage",  &m_LocalCoreStartupParameter.SelectedLanguage, 0);
	core->Get("DPL2Decoder",       &m_LocalCoreStartupParameter.bDPL2Decoder, false);
	core->Get("Latency",           &m_LocalCoreStartupParameter.iLatency, 2);
	core->Get("MemcardAPath",      &m_strMemoryCardA);
	core->Get("MemcardBPath",      &m_strMemoryCardB);
	core->Get("SlotA",       (int*)&m_EXIDevice[0], EXIDEVICE_MEMORYCARD);
	core->Get("SlotB",       (int*)&m_EXIDevice[1], EXIDEVICE_NONE);
	core->Get("SerialPort1", (int*)&m_EXIDevice[2], EXIDEVICE_NONE);
	core->Get("BBA_MAC",           &m_bba_mac);
	core->Get("TimeProfiling",     &m_LocalCoreStartupParameter.bJITILTimeProfiling, false);
	core->Get("OutputIR",          &m_LocalCoreStartupParameter.bJITILOutputIR,      false);
	for (int i = 0; i < MAX_SI_CHANNELS; ++i)
	{
		core->Get(StringFromFormat("SIDevice%i", i), (u32*)&m_SIDevice[i], (i == 0) ? SIDEVICE_GC_CONTROLLER : SIDEVICE_NONE);
	}
	core->Get("WiiSDCard",                 &m_WiiSDCard,                                   false);
	core->Get("WiiKeyboard",               &m_WiiKeyboard,                                 false);
	core->Get("WiimoteContinuousScanning", &m_WiimoteContinuousScanning,                   false);
	core->Get("WiimoteEnableSpeaker",      &m_WiimoteEnableSpeaker,                        false);
	core->Get("RunCompareServer",          &m_LocalCoreStartupParameter.bRunCompareServer, false);
	core->Get("RunCompareClient",          &m_LocalCoreStartupParameter.bRunCompareClient, false);
	core->Get("MMU",                       &m_LocalCoreStartupParameter.bMMU,              false);
	core->Get("BBDumpPort",                &m_LocalCoreStartupParameter.iBBDumpPort,       -1);
	core->Get("VBeam",                     &m_LocalCoreStartupParameter.bVBeamSpeedHack,   false);
	core->Get("SyncGPU",                   &m_LocalCoreStartupParameter.bSyncGPU,          false);
	core->Get("FastDiscSpeed",             &m_LocalCoreStartupParameter.bFastDiscSpeed,    false);
	core->Get("DCBZ",                      &m_LocalCoreStartupParameter.bDCBZOFF,          false);
	core->Get("FrameLimit",                &m_Framelimit,                                  1); // auto frame limit by default
	core->Get("FrameSkip",                 &m_FrameSkip,                                   0);
	core->Get("GFXBackend",                &m_LocalCoreStartupParameter.m_strVideoBackend, "");
	core->Get("GPUDeterminismMode",        &m_LocalCoreStartupParameter.m_strGPUDeterminismMode, "auto");
}

void SConfig::LoadMovieSettings(IniFile& ini)
{
	IniFile::Section* movie = ini.GetOrCreateSection("Movie");

	movie->Get("PauseMovie", &m_PauseMovie, false);
	movie->Get("Author", &m_strMovieAuthor, "");
	movie->Get("DumpFrames", &m_DumpFrames, false);
	movie->Get("ShowInputDisplay", &m_ShowInputDisplay, false);
}

void SConfig::LoadDSPSettings(IniFile& ini)
{
	IniFile::Section* dsp = ini.GetOrCreateSection("DSP");

	dsp->Get("EnableJIT", &m_DSPEnableJIT, true);
	dsp->Get("DumpAudio", &m_DumpAudio, false);
#if defined __linux__ && HAVE_ALSA
	dsp->Get("Backend", &sBackend, BACKEND_ALSA);
#elif defined __APPLE__
	dsp->Get("Backend", &sBackend, BACKEND_COREAUDIO);
#elif defined _WIN32
	dsp->Get("Backend", &sBackend, BACKEND_XAUDIO2);
#elif defined ANDROID
	dsp->Get("Backend", &sBackend, BACKEND_OPENSLES);
#else
	dsp->Get("Backend", &sBackend, BACKEND_NULLSOUND);
#endif
	dsp->Get("Volume", &m_Volume, 100);
	dsp->Get("CaptureLog", &m_DSPCaptureLog, false);
}

void SConfig::LoadInputSettings(IniFile& ini)
{
	IniFile::Section* input = ini.GetOrCreateSection("Input");

	input->Get("BackgroundInput", &m_BackgroundInput, false);
}

void SConfig::LoadFifoPlayerSettings(IniFile& ini)
{
	IniFile::Section* fifoplayer = ini.GetOrCreateSection("FifoPlayer");

	fifoplayer->Get("LoopReplay", &m_LocalCoreStartupParameter.bLoopFifoReplay, true);
}
