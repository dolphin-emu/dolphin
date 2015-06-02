// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/HW/SI.h"
#include "Core/PowerPC/PowerPC.h"
#include "DiscIO/NANDContentLoader.h"

SConfig* SConfig::m_Instance;

static const struct
{
	const char* IniText;
	const bool  KBM;
	const bool  DInput;
	const int   DefaultKey;
	const int   DefaultModifier;
	const u32   DandXInputMapping;
	const u32   DInputMappingExtra;
} g_HKData[] = {
	{ "Open",                 true, false, 79 /* 'O' */,        2 /* wxMOD_CONTROL */, 0, 0 },
	{ "ChangeDisc",           true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "RefreshList",          true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
#ifdef __APPLE__
	{ "PlayPause",            true, false, 80 /* 'P' */,        2 /* wxMOD_CMD */,     0, 0 },
	{ "Stop",                 true, false, 87 /* 'W' */,        2 /* wxMOD_CMD */,     0, 0 },
	{ "Reset",                true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "FrameAdvance",         true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "StartRecording",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "PlayRecording",        true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "ExportRecording",      true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "Readonlymode",         true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "ToggleFullscreen",     true, false, 70 /* 'F' */,        2 /* wxMOD_CMD */,     0, 0 },
	{ "Screenshot",           true, false, 83 /* 'S' */,        2 /* wxMOD_CMD */,     0, 0 },
	{ "Exit",                 true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "Wiimote1Connect",      true, false, 49 /* '1' */,        2 /* wxMOD_CMD */,     0, 0 },
	{ "Wiimote2Connect",      true, false, 50 /* '2' */,        2 /* wxMOD_CMD */,     0, 0 },
	{ "Wiimote3Connect",      true, false, 51 /* '3' */,        2 /* wxMOD_CMD */,     0, 0 },
	{ "Wiimote4Connect",      true, false, 52 /* '4' */,        2 /* wxMOD_CMD */,     0, 0 },
	{ "BalanceBoardConnect",  true, false, 53 /* '4' */,        2 /* wxMOD_CMD */,     0, 0 },
#else
	{ "PlayPause",            true, false, 349 /* WXK_F10 */,   0 /* wxMOD_NONE */,    0, 0 },
	{ "Stop",                 true, false, 27 /* WXK_ESCAPE */, 0 /* wxMOD_NONE */,    0, 0 },
	{ "Reset",                true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "FrameAdvance",         true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "StartRecording",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "PlayRecording",        true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "ExportRecording",      true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "Readonlymode",         true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "ToggleFullscreen",     true, false, 13 /* WXK_RETURN */, 1 /* wxMOD_ALT */,     0, 0 },
	{ "Screenshot",           true, false, 348 /* WXK_F9 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "Exit",                 true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "Wiimote1Connect",      true, false, 344 /* WXK_F5 */,    1 /* wxMOD_ALT */,     0, 0 },
	{ "Wiimote2Connect",      true, false, 345 /* WXK_F6 */,    1 /* wxMOD_ALT */,     0, 0 },
	{ "Wiimote3Connect",      true, false, 346 /* WXK_F7 */,    1 /* wxMOD_ALT */,     0, 0 },
	{ "Wiimote4Connect",      true, false, 347 /* WXK_F8 */,    1 /* wxMOD_ALT */,     0, 0 },
	{ "BalanceBoardConnect",  true, false, 348 /* WXK_F9 */,    1 /* wxMOD_ALT */,     0, 0 },
#endif

	{ "VolumeDown",          true, false, 0,                    0 /* wxMOD_NONE */,    0, 0 },
	{ "VolumeUp",            true, false, 0,                    0 /* wxMOD_NONE */,    0, 0 },
	{ "VolumeToggleMute",    true, false, 0,                    0 /* wxMOD_NONE */,    0, 0 },

	{ "IncreaseIR",           true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "DecreaseIR",           true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "ToggleIR",             true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "ToggleAspectRatio",    true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "ToggleEFBCopies",      true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "ToggleFog",            true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "ToggleThrottle",       true, false, 9 /* '\t' */,        0 /* wxMOD_NONE */,    0, 0 },
	{ "DecreaseFrameLimit",   true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "IncreaseFrameLimit",   true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "FreelookDecreaseSpeed", true, false, 49 /* '1' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookIncreaseSpeed", true, false, 50 /* '2' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookResetSpeed",    true, false, 70 /* 'F' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookUp",            true, false, 69 /* 'E' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookDown",          true, false, 81 /* 'Q' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookLeft",          true, false, 65 /* 'A' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookRight",         true, false, 68 /* 'D' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookZoomIn",        true, false, 87 /* 'W' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookZoomOut",       true, false, 83 /* 'S' */,       4 /* wxMOD_SHIFT */,   0, 0 },
	{ "FreelookReset",         true, false, 82 /* 'R' */,       4 /* wxMOD_SHIFT */,   0, 0 },

	{ "DecreaseDepth",        true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "IncreaseDepth",        true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "DecreaseConvergence",  true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "IncreaseConvergence",  true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "LoadStateSlot1",       true, false, 340 /* WXK_F1 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot2",       true, false, 341 /* WXK_F2 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot3",       true, false, 342 /* WXK_F3 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot4",       true, false, 343 /* WXK_F4 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot5",       true, false, 344 /* WXK_F5 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot6",       true, false, 345 /* WXK_F6 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot7",       true, false, 346 /* WXK_F7 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot8",       true, false, 347 /* WXK_F8 */,    0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot9",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateSlot10",      true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "SaveStateSlot1",       true, false, 340 /* WXK_F1 */,    4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateSlot2",       true, false, 341 /* WXK_F2 */,    4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateSlot3",       true, false, 342 /* WXK_F3 */,    4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateSlot4",       true, false, 343 /* WXK_F4 */,    4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateSlot5",       true, false, 344 /* WXK_F5 */,    4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateSlot6",       true, false, 345 /* WXK_F6 */,    4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateSlot7",       true, false, 346 /* WXK_F7 */,    4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateSlot8",       true, false, 347 /* WXK_F8 */,    4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateSlot9",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "SaveStateSlot10",      true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "SelectStateSlot1",	  true, false, 0,                    0,                     0, 0 },
	{ "SelectStateSlot2",	  true, false, 0,	                 0,                     0, 0 },
	{ "SelectStateSlot3",	  true, false, 0,	                 0,                     0, 0 },
	{ "SelectStateSlot4",	  true, false, 0,	                 0,                     0, 0 },
	{ "SelectStateSlot5",	  true, false, 0,	                 0,                     0, 0 },
	{ "SelectStateSlot6",	  true, false, 0,	                 0,                     0, 0 },
	{ "SelectStateSlot7",	  true, false, 0,	                 0,                     0, 0 },
	{ "SelectStateSlot8",	  true, false, 0,	                 0,                     0, 0 },
	{ "SelectStateSlot9",	  true, false, 0,	                 0,                     0, 0 },
	{ "SelectStateSlot10",	  true, false, 0,	                 0,                     0, 0 },
	{ "SaveSelectedSlot",	  true, false, 0,	                 0,                     0, 0 },
	{ "LoadSelectedSlot",	  true, false, 0,	                 0,                     0, 0 },
	 
	{ "LoadLastState1",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadLastState2",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadLastState3",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadLastState4",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadLastState5",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadLastState6",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadLastState7",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadLastState8",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },

	{ "SaveFirstState",       true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "UndoLoadState",        true, false, 351 /* WXK_F12 */,   0 /* wxMOD_NONE */,    0, 0 },
	{ "UndoSaveState",        true, false, 351 /* WXK_F12 */,   4 /* wxMOD_SHIFT */,   0, 0 },
	{ "SaveStateFile",        true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
	{ "LoadStateFile",        true, false, 0,                   0 /* wxMOD_NONE */,    0, 0 },
};

static const struct
{
	const char* IniText;
	const bool  KBM;
	const bool  DInput;
	const int   DefaultKey;
	const int   DefaultModifier;
	const u32   DandXInputMapping;
	const u32   DInputMappingExtra;
} g_VRData[] = {
		{ "FreelookReset",              true, false, 82, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "FreelookZoomIn",             true, false, 87, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "FreelookZoomOut",            true, false, 83, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "FreelookLeft",               true, false, 65, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "FreelookRight",              true, false, 68, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "FreelookUp",                 true, false, 69, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "FreelookDown",               true, false, 81, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRPermanentCameraForward",   true, false, 80, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRPermanentCameraBackward",  true, false, 59, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRLargerScale",              true, false, 61, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRSmallerScale",	            true, false, 45, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRGlobalLargerScale",        true, false,  0, 0 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRGlobalSmallerScale",       true, false,  0, 0 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRCameraTiltUp",             true, false, 79, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRCameraTiltDown",           true, false, 76, 4 /* wxMOD_SHIFT */, 0, 0 },

		{ "VRHUDForward",               true, false, 47, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRHUDBackward",              true, false, 46, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRHUDThicker",               true, false, 93, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRHUDThinner",               true, false, 91, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VRHUD3DCloser",              true, false,  0, 0 /* wxMOD_NONE */, 0, 0 },
		{ "VRHUD3DFurther",             true, false,  0, 0 /* wxMOD_NONE */, 0, 0 },

		{ "VR2DScreenLarger",           true, false, 44, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VR2DScreenSmaller",          true, false, 77, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VR2DCameraForward",          true, false, 74, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VR2DCameraBackward",         true, false, 85, 4 /* wxMOD_SHIFT */, 0, 0 },
		//{ "VR2DScreenLeft",             true, false, 0, 0 /* wxMOD_NONE */, 0, 0 }, //doesn't_exist_right_now?
		//{ "VR2DScreenRight",            true, false, 0, 0 /* wxMOD_NONE */, 0, 0 }, //doesn't_exist_right_now?
		{ "VR2DCameraUp",               true, false, 72, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VR2DCameraDown",             true, false, 89, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VR2DCameraTiltUp",           true, false, 73, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VR2DCameraTiltDown",         true, false, 75, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VR2DScreenThicker",          true, false, 84, 4 /* wxMOD_SHIFT */, 0, 0 },
		{ "VR2DScreenThinner",          true, false, 71, 4 /* wxMOD_SHIFT */, 0, 0 },

};

GPUDeterminismMode ParseGPUDeterminismMode(const std::string& mode)
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
	if (!ARBruteForcer::ch_dont_save_settings)
		SaveDisplaySettings(ini);
	SaveGameListSettings(ini);
	SaveCoreSettings(ini);
	SaveMovieSettings(ini);
	SaveDSPSettings(ini);
	SaveInputSettings(ini);
	SaveFifoPlayerSettings(ini);
	SaveVRSettings(ini);

	ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	m_SYSCONF->Save();
}

void SConfig::SaveSingleSetting(std::string section_name, std::string setting_name, float value_to_save)
{
	IniFile iniFile;
	iniFile.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");

	IniFile::Section* vr = iniFile.GetOrCreateSection(section_name);
	vr->Set(setting_name, value_to_save);
	iniFile.Save(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
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
	general->Get("GCMPathes", &oldPaths, 0);
	for (int i = 0; i < oldPaths; i++)
	{
		ini.DeleteKey("General", StringFromFormat("GCMPath%i", i));
	}
	ini.DeleteKey("General", "GCMPathes");
	ini.DeleteKey("General", "RecursiveGCMPaths");

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
#ifndef _WIN32
	general->Set("GDBSocket", m_LocalCoreStartupParameter.gdb_socket);
#endif
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
	interface->Set("PauseOnFocusLost", m_PauseOnFocusLost);
}

void SConfig::SaveHotkeySettings(IniFile& ini)
{
	IniFile::Section* hotkeys = ini.GetOrCreateSection("Hotkeys");

	hotkeys->Set("XInputPolling", m_LocalCoreStartupParameter.bHotkeysXInput);

	for (int i = 0; i < NUM_HOTKEYS; i++)
	{
		hotkeys->Set(g_HKData[i].IniText, m_LocalCoreStartupParameter.iHotkey[i]);
		hotkeys->Set(std::string(g_HKData[i].IniText) + "Modifier",
			m_LocalCoreStartupParameter.iHotkeyModifier[i]);
		hotkeys->Set(std::string(g_HKData[i].IniText) + "KBM",
			m_LocalCoreStartupParameter.bHotkeyKBM[i]);
		hotkeys->Set(std::string(g_HKData[i].IniText) + "DInput",
			m_LocalCoreStartupParameter.bHotkeyDInput[i]);
		hotkeys->Set(std::string(g_HKData[i].IniText) + "XInputMapping",
			m_LocalCoreStartupParameter.iHotkeyDandXInputMapping[i]);
		hotkeys->Set(std::string(g_HKData[i].IniText) + "DInputMappingExtra",
			m_LocalCoreStartupParameter.iHotkeyDInputMappingExtra[i]);
	}
}

void SConfig::SaveVRSettings(IniFile& ini)
{
	IniFile::Section* vrsettings = ini.GetOrCreateSection("Hotkeys");

	for (int i = 0; i < NUM_VR_HOTKEYS; i++)
	{
		vrsettings->Set(g_VRData[i].IniText, m_LocalCoreStartupParameter.iVRSettings[i]);
		vrsettings->Set(std::string(g_VRData[i].IniText) + "Modifier",
			m_LocalCoreStartupParameter.iVRSettingsModifier[i]);
		vrsettings->Set(std::string(g_VRData[i].IniText) + "KBM",
			m_LocalCoreStartupParameter.bVRSettingsKBM[i]);
		vrsettings->Set(std::string(g_VRData[i].IniText) + "DInput",
			m_LocalCoreStartupParameter.bVRSettingsDInput[i]);
		vrsettings->Set(std::string(g_VRData[i].IniText) + "XInputMapping",
			m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[i]);
		vrsettings->Set(std::string(g_VRData[i].IniText) + "DInputMappingExtra",
			m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[i]);
	}
}

void SConfig::SaveDisplaySettings(IniFile& ini)
{
	IniFile::Section* display = ini.GetOrCreateSection("Display");

	if (!m_special_case)
		display->Set("FullscreenResolution", m_LocalCoreStartupParameter.strFullscreenResolution);
	display->Set("Fullscreen", m_LocalCoreStartupParameter.bFullscreen);
	display->Set("RenderToMain", m_LocalCoreStartupParameter.bRenderToMain);
	if (!m_special_case)
	{
		display->Set("RenderWindowXPos", m_LocalCoreStartupParameter.iRenderWindowXPos);
		display->Set("RenderWindowYPos", m_LocalCoreStartupParameter.iRenderWindowYPos);
		display->Set("RenderWindowWidth", m_LocalCoreStartupParameter.iRenderWindowWidth);
		display->Set("RenderWindowHeight", m_LocalCoreStartupParameter.iRenderWindowHeight);
	}
	display->Set("RenderWindowAutoSize", m_LocalCoreStartupParameter.bRenderWindowAutoSize);
	display->Set("KeepWindowOnTop", m_LocalCoreStartupParameter.bKeepWindowOnTop);
	display->Set("ProgressiveScan", m_LocalCoreStartupParameter.bProgressive);
	display->Set("DisableScreenSaver", m_LocalCoreStartupParameter.bDisableScreenSaver);
	display->Set("ForceNTSCJ", m_LocalCoreStartupParameter.bForceNTSCJ);

	IniFile::Section* vr = ini.GetOrCreateSection("VR");
#ifdef OCULUSSDK042
	vr->Set("AsynchronousTimewarp", m_LocalCoreStartupParameter.bAsynchronousTimewarp);
#endif
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
	gamelist->Set("ListAustralia", m_ListAustralia);
	gamelist->Set("ListFrance", m_ListFrance);
	gamelist->Set("ListGermany", m_ListGermany);
	gamelist->Set("ListItaly", m_ListItaly);
	gamelist->Set("ListKorea", m_ListKorea);
	gamelist->Set("ListNetherlands", m_ListNetherlands);
	gamelist->Set("ListRussia", m_ListRussia);
	gamelist->Set("ListSpain", m_ListSpain);
	gamelist->Set("ListTaiwan", m_ListTaiwan);
	gamelist->Set("ListWorld", m_ListWorld);
	gamelist->Set("ListUnknown", m_ListUnknown);
	gamelist->Set("ListSort", m_ListSort);
	gamelist->Set("ListSortSecondary", m_ListSort2);

	gamelist->Set("ColorCompressed", m_ColorCompressed);

	gamelist->Set("ColumnPlatform", m_showSystemColumn);
	gamelist->Set("ColumnBanner", m_showBannerColumn);
	gamelist->Set("ColumnNotes", m_showMakerColumn);
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
	core->Set("DSPHLE", m_LocalCoreStartupParameter.bDSPHLE);
	core->Set("SkipIdle", m_LocalCoreStartupParameter.bSkipIdle);
	core->Set("SyncOnSkipIdle", m_LocalCoreStartupParameter.bSyncGPUOnSkipIdleHack);
	core->Set("DefaultISO", m_LocalCoreStartupParameter.m_strDefaultISO);
	core->Set("DVDRoot", m_LocalCoreStartupParameter.m_strDVDRoot);
	core->Set("Apploader", m_LocalCoreStartupParameter.m_strApploader);
	core->Set("EnableCheats", m_LocalCoreStartupParameter.bEnableCheats);
	core->Set("SelectedLanguage", m_LocalCoreStartupParameter.SelectedLanguage);
	core->Set("DPL2Decoder", m_LocalCoreStartupParameter.bDPL2Decoder);
	core->Set("Latency", m_LocalCoreStartupParameter.iLatency);
	core->Set("MemcardAPath", m_strMemoryCardA);
	core->Set("MemcardBPath", m_strMemoryCardB);
	core->Set("AgpCartAPath", m_strGbaCartA);
	core->Set("AgpCartBPath", m_strGbaCartB);
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
	if (!ARBruteForcer::ch_dont_save_settings)
		core->Set("FrameLimit", m_Framelimit);
	core->Set("FrameSkip", m_FrameSkip);
	core->Set("Overclock", m_OCFactor);
	core->Set("OverclockEnable", m_OCEnable);
	core->Set("GFXBackend", m_LocalCoreStartupParameter.m_strVideoBackend);
	core->Set("GPUDeterminismMode", m_LocalCoreStartupParameter.m_strGPUDeterminismMode);
	core->Set("GameCubeAdapter", m_GameCubeAdapter);
	core->Set("AdapterRumble", m_AdapterRumble);
}

void SConfig::SaveMovieSettings(IniFile& ini)
{
	IniFile::Section* movie = ini.GetOrCreateSection("Movie");

	movie->Set("PauseMovie", m_PauseMovie);
	movie->Set("Author", m_strMovieAuthor);
	movie->Set("DumpFrames", m_DumpFrames);
	movie->Set("DumpFramesSilent", m_DumpFramesSilent);
	movie->Set("ShowInputDisplay", m_ShowInputDisplay);
}

void SConfig::SaveDSPSettings(IniFile& ini)
{
	IniFile::Section* dsp = ini.GetOrCreateSection("DSP");

	dsp->Set("EnableJIT", m_DSPEnableJIT);
	dsp->Set("DumpAudio", m_DumpAudio);
	if (!ARBruteForcer::ch_dont_save_settings)
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
	LoadVRSettings(ini);

	m_SYSCONF = new SysConf();
}

void SConfig::LoadGeneralSettings(IniFile& ini)
{
	IniFile::Section* general = ini.GetOrCreateSection("General");

	general->Get("LastFilename", &m_LastFilename);
	general->Get("ShowLag", &m_ShowLag, false);
	general->Get("ShowFrameCount", &m_ShowFrameCount, false);
#ifdef USE_GDBSTUB
#ifndef _WIN32
	general->Get("GDBSocket", &m_LocalCoreStartupParameter.gdb_socket, "");
#endif
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
	// Check for old file path (Changed in 4.0-4003)
	// This can probably be removed after 5.0 stable is launched
	else if (general->Get("GCMPathes", &numISOPaths, 0) && numISOPaths > 0)
	{
		for (int i = 0; i < numISOPaths; i++)
		{
			std::string tmpPath;
			general->Get(StringFromFormat("GCMPath%i", i), &tmpPath, "");
			bool found = false;
			for (size_t j = 0; j < m_ISOFolder.size(); ++j)
			{
				if (m_ISOFolder[j] == tmpPath)
				{
					found = true;
					break;
				}
			}
			if (!found)
				m_ISOFolder.push_back(std::move(tmpPath));
		}
	}

	if (!general->Get("RecursiveISOPaths", &m_RecursiveISOFolder, false) || !m_RecursiveISOFolder)
	{
		// Check for old name
		general->Get("RecursiveGCMPaths", &m_RecursiveISOFolder, false);
	}

	general->Get("NANDRootPath", &m_NANDPath);
	File::SetUserPath(D_WIIROOT_IDX, m_NANDPath);
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
	interface->Get("PauseOnFocusLost",        &m_PauseOnFocusLost,                            false);
}

void SConfig::LoadHotkeySettings(IniFile& ini)
{
	IniFile::Section* hotkeys = ini.GetOrCreateSection("Hotkeys");

	hotkeys->Get("XInputPolling", &m_LocalCoreStartupParameter.bHotkeysXInput, true);

	for (int i = 0; i < NUM_HOTKEYS; i++)
	{
		hotkeys->Get(g_HKData[i].IniText,
		    &m_LocalCoreStartupParameter.iHotkey[i], 0);
		hotkeys->Get(std::string(g_HKData[i].IniText) + "Modifier",
		    &m_LocalCoreStartupParameter.iHotkeyModifier[i], 0);
		hotkeys->Get(std::string(g_HKData[i].IniText) + "KBM",
			&m_LocalCoreStartupParameter.bHotkeyKBM[i], g_HKData[i].KBM);
		hotkeys->Get(std::string(g_HKData[i].IniText) + "DInput",
			&m_LocalCoreStartupParameter.bHotkeyDInput[i], g_HKData[i].DInput);
		hotkeys->Get(std::string(g_HKData[i].IniText) + "XInputMapping",
			&m_LocalCoreStartupParameter.iHotkeyDandXInputMapping[i], g_HKData[i].DandXInputMapping);
		hotkeys->Get(std::string(g_HKData[i].IniText) + "DInputMappingExtra",
			&m_LocalCoreStartupParameter.iHotkeyDInputMappingExtra[i], g_HKData[i].DInputMappingExtra);
	}
}

void SConfig::LoadVRSettings(IniFile& ini)
{
	IniFile::Section* vrsettings = ini.GetOrCreateSection("Hotkeys");


	for (int i = 0; i < NUM_VR_HOTKEYS; i++)
	{
		vrsettings->Get(g_VRData[i].IniText,
			&m_LocalCoreStartupParameter.iVRSettings[i], g_VRData[i].DefaultKey);
		vrsettings->Get(std::string(g_VRData[i].IniText) + "Modifier",
			&m_LocalCoreStartupParameter.iVRSettingsModifier[i], g_VRData[i].DefaultModifier);
		vrsettings->Get(std::string(g_VRData[i].IniText) + "KBM",
			&m_LocalCoreStartupParameter.bVRSettingsKBM[i], g_VRData[i].KBM);
		vrsettings->Get(std::string(g_VRData[i].IniText) + "DInput",
			&m_LocalCoreStartupParameter.bVRSettingsDInput[i], g_VRData[i].DInput);
		vrsettings->Get(std::string(g_VRData[i].IniText) + "XInputMapping",
			&m_LocalCoreStartupParameter.iVRSettingsDandXInputMapping[i], g_VRData[i].DandXInputMapping);
		vrsettings->Get(std::string(g_VRData[i].IniText) + "DInputMappingExtra",
			&m_LocalCoreStartupParameter.iVRSettingsDInputMappingExtra[i], g_VRData[i].DInputMappingExtra);
	}
}

void SConfig::LoadDisplaySettings(IniFile& ini)
{
	IniFile::Section* display = ini.GetOrCreateSection("Display");

	if (ARBruteForcer::ch_bruteforce)
	{
		m_LocalCoreStartupParameter.bFullscreen = false;
		m_LocalCoreStartupParameter.strFullscreenResolution = "Auto";
		m_LocalCoreStartupParameter.bRenderToMain = false;
		m_LocalCoreStartupParameter.iRenderWindowXPos = -1;
		m_LocalCoreStartupParameter.iRenderWindowYPos = -1;
		m_LocalCoreStartupParameter.iRenderWindowWidth = 640;
		m_LocalCoreStartupParameter.iRenderWindowHeight = 480;
		m_LocalCoreStartupParameter.bRenderWindowAutoSize = false;
		m_LocalCoreStartupParameter.bKeepWindowOnTop = false;
		m_LocalCoreStartupParameter.bProgressive = false;
		m_LocalCoreStartupParameter.bDisableScreenSaver = true;
		m_LocalCoreStartupParameter.bForceNTSCJ = false;
	}
	else
	{
		display->Get("Fullscreen", &m_LocalCoreStartupParameter.bFullscreen, false);
		display->Get("FullscreenResolution", &m_LocalCoreStartupParameter.strFullscreenResolution, "Auto");
		display->Get("RenderToMain", &m_LocalCoreStartupParameter.bRenderToMain, false);
		display->Get("RenderWindowXPos", &m_LocalCoreStartupParameter.iRenderWindowXPos, -1);
		display->Get("RenderWindowYPos", &m_LocalCoreStartupParameter.iRenderWindowYPos, -1);
		display->Get("RenderWindowWidth", &m_LocalCoreStartupParameter.iRenderWindowWidth, 640);
		display->Get("RenderWindowHeight", &m_LocalCoreStartupParameter.iRenderWindowHeight, 480);
		display->Get("RenderWindowAutoSize", &m_LocalCoreStartupParameter.bRenderWindowAutoSize, false);
		display->Get("KeepWindowOnTop", &m_LocalCoreStartupParameter.bKeepWindowOnTop, false);
		display->Get("ProgressiveScan", &m_LocalCoreStartupParameter.bProgressive, false);
		display->Get("DisableScreenSaver", &m_LocalCoreStartupParameter.bDisableScreenSaver, true);
		display->Get("ForceNTSCJ", &m_LocalCoreStartupParameter.bForceNTSCJ, false);
	}

	IniFile::Section* vr = ini.GetOrCreateSection("VR");
#ifdef OCULUSSDK042
	vr->Get("AsynchronousTimewarp",      &m_LocalCoreStartupParameter.bAsynchronousTimewarp,   false);
#else
	m_LocalCoreStartupParameter.bAsynchronousTimewarp = false;
#endif
}

void SConfig::LoadGameListSettings(IniFile& ini)
{
	IniFile::Section* gamelist = ini.GetOrCreateSection("GameList");

	gamelist->Get("ListDrives",        &m_ListDrives,  false);
	gamelist->Get("ListWad",           &m_ListWad,     true);
	gamelist->Get("ListWii",           &m_ListWii,     true);
	gamelist->Get("ListGC",            &m_ListGC,      true);
	gamelist->Get("ListJap",           &m_ListJap,     true);
	gamelist->Get("ListPal",           &m_ListPal,     true);
	gamelist->Get("ListUsa",           &m_ListUsa,     true);

	gamelist->Get("ListAustralia",     &m_ListAustralia,     true);
	gamelist->Get("ListFrance",        &m_ListFrance,        true);
	gamelist->Get("ListGermany",       &m_ListGermany,       true);
	gamelist->Get("ListItaly",         &m_ListItaly,         true);
	gamelist->Get("ListKorea",         &m_ListKorea,         true);
	gamelist->Get("ListNetherlands",   &m_ListNetherlands,   true);
	gamelist->Get("ListRussia",        &m_ListRussia,        true);
	gamelist->Get("ListSpain",         &m_ListSpain,         true);
	gamelist->Get("ListTaiwan",        &m_ListTaiwan,        true);
	gamelist->Get("ListWorld",         &m_ListWorld,         true);
	gamelist->Get("ListUnknown",       &m_ListUnknown,       true);
	gamelist->Get("ListSort",          &m_ListSort,       3);
	gamelist->Get("ListSortSecondary", &m_ListSort2,      0);

	// Determines if compressed games display in blue
	gamelist->Get("ColorCompressed", &m_ColorCompressed, true);

	// Gamelist columns toggles
	gamelist->Get("ColumnPlatform",   &m_showSystemColumn,  true);
	gamelist->Get("ColumnBanner",     &m_showBannerColumn,  true);
	gamelist->Get("ColumnNotes",      &m_showMakerColumn,   true);
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
	core->Get("CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, PowerPC::CORE_JIT64);
#elif _M_ARM_32
	core->Get("CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, PowerPC::CORE_JITARM);
#elif _M_ARM_64
	core->Get("CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, PowerPC::CORE_JITARM64);
#else
	core->Get("CPUCore",      &m_LocalCoreStartupParameter.iCPUCore, PowerPC::CORE_INTERPRETER);
#endif
	core->Get("Fastmem",           &m_LocalCoreStartupParameter.bFastmem,      true);
	core->Get("DSPHLE",            &m_LocalCoreStartupParameter.bDSPHLE,       true);
	core->Get("CPUThread",         &m_LocalCoreStartupParameter.bCPUThread,    true);
	core->Get("SkipIdle",          &m_LocalCoreStartupParameter.bSkipIdle,     true);
	core->Get("SyncOnSkipIdle",    &m_LocalCoreStartupParameter.bSyncGPUOnSkipIdleHack, true);
	core->Get("DefaultISO",        &m_LocalCoreStartupParameter.m_strDefaultISO);
	core->Get("DVDRoot",           &m_LocalCoreStartupParameter.m_strDVDRoot);
	core->Get("Apploader",         &m_LocalCoreStartupParameter.m_strApploader);
	core->Get("EnableCheats",      &m_LocalCoreStartupParameter.bEnableCheats, false);
	core->Get("SelectedLanguage",  &m_LocalCoreStartupParameter.SelectedLanguage, 0);
	core->Get("DPL2Decoder",       &m_LocalCoreStartupParameter.bDPL2Decoder, false);
	core->Get("Latency",           &m_LocalCoreStartupParameter.iLatency, 2);
	core->Get("MemcardAPath",      &m_strMemoryCardA);
	core->Get("MemcardBPath",      &m_strMemoryCardB);
	core->Get("AgpCartAPath",      &m_strGbaCartA);
	core->Get("AgpCartBPath",      &m_strGbaCartB);
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
	core->Get("SyncGPU",                   &m_LocalCoreStartupParameter.bSyncGPU,          false);
	core->Get("FastDiscSpeed",             &m_LocalCoreStartupParameter.bFastDiscSpeed,    false);
	core->Get("DCBZ",                      &m_LocalCoreStartupParameter.bDCBZOFF,          false);
	if (ARBruteForcer::ch_bruteforce)
		m_Framelimit = 0;
	else
		core->Get("FrameLimit",                &m_Framelimit,                                  1); // auto frame limit by default
	core->Get("Overclock",                 &m_OCFactor,                                    1.0f);
	core->Get("OverclockEnable",           &m_OCEnable,                                    false);
	core->Get("FrameSkip",                 &m_FrameSkip,                                   0);
	core->Get("GFXBackend",                &m_LocalCoreStartupParameter.m_strVideoBackend, "");
	core->Get("GPUDeterminismMode",        &m_LocalCoreStartupParameter.m_strGPUDeterminismMode, "auto");
	m_LocalCoreStartupParameter.m_GPUDeterminismMode = ParseGPUDeterminismMode(m_LocalCoreStartupParameter.m_strGPUDeterminismMode);
	core->Get("GameCubeAdapter",           &m_GameCubeAdapter,                             true);
	core->Get("AdapterRumble",             &m_AdapterRumble,                               true);
}

void SConfig::LoadMovieSettings(IniFile& ini)
{
	IniFile::Section* movie = ini.GetOrCreateSection("Movie");

	movie->Get("PauseMovie", &m_PauseMovie, false);
	movie->Get("Author", &m_strMovieAuthor, "");
	movie->Get("DumpFrames", &m_DumpFrames, false);
	movie->Get("DumpFramesSilent", &m_DumpFramesSilent, false);
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

	if (ARBruteForcer::ch_bruteforce)
		sBackend = BACKEND_NULLSOUND;

	m_IsMuted = false;
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
