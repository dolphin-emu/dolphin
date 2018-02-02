// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"

#include <cinttypes>
#include <climits>
#include <memory>
#include <optional>
#include <sstream>
#include <variant>

#include "AudioCommon/AudioCommon.h"

#include "Common/Assert.h"
#include "Common/CDUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

#include "Core/Analytics.h"
#include "Core/ARBruteForcer.h"
#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/Core.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/HotkeyManager.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/TitleDatabase.h"
#include "VideoCommon/HiresTextures.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/WiiWad.h"

SConfig* SConfig::m_Instance;

static const struct
{
  const char* IniText;
  const bool KBM;
  const bool DInput;
  const int DefaultKey;
  const int DefaultModifier;
  const u32 DandXInputMapping;
  const u32 DInputMappingExtra;
} g_HKData[] = {
    {"Open", true, false, 79 /* 'O' */, 2 /* wxMOD_CONTROL */, 0, 0},
    {"ChangeDisc", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"RefreshList", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
#ifdef __APPLE__
    {"PlayPause", true, false, 80 /* 'P' */, 2 /* wxMOD_CMD */, 0, 0},
    {"Stop", true, false, 87 /* 'W' */, 2 /* wxMOD_CMD */, 0, 0},
    {"Reset", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"FrameAdvance", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"StartRecording", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"PlayRecording", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"ExportRecording", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"Readonlymode", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"ToggleFullscreen", true, false, 70 /* 'F' */, 2 /* wxMOD_CMD */, 0, 0},
    {"Screenshot", true, false, 83 /* 'S' */, 2 /* wxMOD_CMD */, 0, 0},
    {"Exit", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"Wiimote1Connect", true, false, 49 /* '1' */, 2 /* wxMOD_CMD */, 0, 0},
    {"Wiimote2Connect", true, false, 50 /* '2' */, 2 /* wxMOD_CMD */, 0, 0},
    {"Wiimote3Connect", true, false, 51 /* '3' */, 2 /* wxMOD_CMD */, 0, 0},
    {"Wiimote4Connect", true, false, 52 /* '4' */, 2 /* wxMOD_CMD */, 0, 0},
    {"BalanceBoardConnect", true, false, 53 /* '4' */, 2 /* wxMOD_CMD */, 0, 0},
#else
    {"PlayPause", true, false, 349 /* WXK_F10 */, 0 /* wxMOD_NONE */, 0, 0},
    {"Stop", true, false, 27 /* WXK_ESCAPE */, 0 /* wxMOD_NONE */, 0, 0},
    {"Reset", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"FrameAdvance", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"StartRecording", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"PlayRecording", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"ExportRecording", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"Readonlymode", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"ToggleFullscreen", true, false, 13 /* WXK_RETURN */, 1 /* wxMOD_ALT */, 0, 0},
    {"Screenshot", true, false, 348 /* WXK_F9 */, 0 /* wxMOD_NONE */, 0, 0},
    {"Exit", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"Wiimote1Connect", true, false, 344 /* WXK_F5 */, 1 /* wxMOD_ALT */, 0, 0},
    {"Wiimote2Connect", true, false, 345 /* WXK_F6 */, 1 /* wxMOD_ALT */, 0, 0},
    {"Wiimote3Connect", true, false, 346 /* WXK_F7 */, 1 /* wxMOD_ALT */, 0, 0},
    {"Wiimote4Connect", true, false, 347 /* WXK_F8 */, 1 /* wxMOD_ALT */, 0, 0},
    {"BalanceBoardConnect", true, false, 348 /* WXK_F9 */, 1 /* wxMOD_ALT */, 0, 0},
#endif

    {"VolumeDown", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"VolumeUp", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"VolumeToggleMute", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"IncreaseIR", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"DecreaseIR", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"ToggleIR", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"ToggleAspectRatio", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"ToggleEFBCopies", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"ToggleFog", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"ToggleThrottle", true, false, 9 /* '\t' */, 0 /* wxMOD_NONE */, 0, 0},
    {"DecreaseFrameLimit", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"IncreaseFrameLimit", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"FreelookDecreaseSpeed", true, false, 49 /* '1' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookIncreaseSpeed", true, false, 50 /* '2' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookResetSpeed", true, false, 70 /* 'F' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookUp", true, false, 69 /* 'E' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookDown", true, false, 81 /* 'Q' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookLeft", true, false, 65 /* 'A' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookRight", true, false, 68 /* 'D' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookZoomIn", true, false, 87 /* 'W' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookZoomOut", true, false, 83 /* 'S' */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookReset", true, false, 82 /* 'R' */, 4 /* wxMOD_SHIFT */, 0, 0},

    {"DecreaseDepth", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"IncreaseDepth", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"DecreaseConvergence", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"IncreaseConvergence", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"LoadStateSlot1", true, false, 340 /* WXK_F1 */, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot2", true, false, 341 /* WXK_F2 */, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot3", true, false, 342 /* WXK_F3 */, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot4", true, false, 343 /* WXK_F4 */, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot5", true, false, 344 /* WXK_F5 */, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot6", true, false, 345 /* WXK_F6 */, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot7", true, false, 346 /* WXK_F7 */, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot8", true, false, 347 /* WXK_F8 */, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot9", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateSlot10", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"SaveStateSlot1", true, false, 340 /* WXK_F1 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateSlot2", true, false, 341 /* WXK_F2 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateSlot3", true, false, 342 /* WXK_F3 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateSlot4", true, false, 343 /* WXK_F4 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateSlot5", true, false, 344 /* WXK_F5 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateSlot6", true, false, 345 /* WXK_F6 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateSlot7", true, false, 346 /* WXK_F7 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateSlot8", true, false, 347 /* WXK_F8 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateSlot9", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"SaveStateSlot10", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"SelectStateSlot1", true, false, 0, 0, 0, 0},
    {"SelectStateSlot2", true, false, 0, 0, 0, 0},
    {"SelectStateSlot3", true, false, 0, 0, 0, 0},
    {"SelectStateSlot4", true, false, 0, 0, 0, 0},
    {"SelectStateSlot5", true, false, 0, 0, 0, 0},
    {"SelectStateSlot6", true, false, 0, 0, 0, 0},
    {"SelectStateSlot7", true, false, 0, 0, 0, 0},
    {"SelectStateSlot8", true, false, 0, 0, 0, 0},
    {"SelectStateSlot9", true, false, 0, 0, 0, 0},
    {"SelectStateSlot10", true, false, 0, 0, 0, 0},
    {"SaveSelectedSlot", true, false, 0, 0, 0, 0},
    {"LoadSelectedSlot", true, false, 0, 0, 0, 0},

    {"LoadLastState1", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadLastState2", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadLastState3", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadLastState4", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadLastState5", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadLastState6", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadLastState7", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadLastState8", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"SaveFirstState", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"UndoLoadState", true, false, 351 /* WXK_F12 */, 0 /* wxMOD_NONE */, 0, 0},
    {"UndoSaveState", true, false, 351 /* WXK_F12 */, 4 /* wxMOD_SHIFT */, 0, 0},
    {"SaveStateFile", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"LoadStateFile", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
};

static const struct
{
  const char* IniText;
  const bool KBM;
  const bool DInput;
  const int DefaultKey;
  const int DefaultModifier;
  const u32 DandXInputMapping;
  const u32 DInputMappingExtra;
} g_VRData[] = {
    {"FreelookReset", true, false, 82, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookZoomIn", true, false, 87, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookZoomOut", true, false, 83, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookLeft", true, false, 65, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookRight", true, false, 68, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookUp", true, false, 69, 4 /* wxMOD_SHIFT */, 0, 0},
    {"FreelookDown", true, false, 81, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRPermanentCameraForward", true, false, 80, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRPermanentCameraBackward", true, false, 59, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRLargerScale", true, false, 61, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRSmallerScale", true, false, 45, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRGlobalLargerScale", true, false, 0, 0 /* wxMOD_SHIFT */, 0, 0},
    {"VRGlobalSmallerScale", true, false, 0, 0 /* wxMOD_SHIFT */, 0, 0},
    {"VRCameraTiltUp", true, false, 79, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRCameraTiltDown", true, false, 76, 4 /* wxMOD_SHIFT */, 0, 0},

    {"VRHUDForward", true, false, 47, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRHUDBackward", true, false, 46, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRHUDThicker", true, false, 93, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRHUDThinner", true, false, 91, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VRHUD3DCloser", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},
    {"VRHUD3DFurther", true, false, 0, 0 /* wxMOD_NONE */, 0, 0},

    {"VR2DScreenLarger", true, false, 44, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VR2DScreenSmaller", true, false, 77, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VR2DCameraForward", true, false, 74, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VR2DCameraBackward", true, false, 85, 4 /* wxMOD_SHIFT */, 0, 0},
    //{ "VR2DScreenLeft",             true, false, 0, 0 /* wxMOD_NONE */, 0, 0 },
    ////doesn't_exist_right_now?
    //{ "VR2DScreenRight",            true, false, 0, 0 /* wxMOD_NONE */, 0, 0 },
    ////doesn't_exist_right_now?
    {"VR2DCameraUp", true, false, 72, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VR2DCameraDown", true, false, 89, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VR2DCameraTiltUp", true, false, 73, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VR2DCameraTiltDown", true, false, 75, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VR2DScreenThicker", true, false, 84, 4 /* wxMOD_SHIFT */, 0, 0},
    {"VR2DScreenThinner", true, false, 71, 4 /* wxMOD_SHIFT */, 0, 0},

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
  LoadDefaults();
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
}

void SConfig::SaveSettings()
{
  NOTICE_LOG(BOOT, "Saving settings to %s", File::GetUserPath(F_DOLPHINCONFIG_IDX).c_str());
  IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));  // load first to not kill unknown stuff

  SaveGeneralSettings(ini);
  SaveInterfaceSettings(ini);
  if (!ARBruteForcer::ch_dont_save_settings)
    SaveDisplaySettings(ini);
  SaveGameListSettings(ini);
  SaveCoreSettings(ini);
  SaveMovieSettings(ini);
  SaveDSPSettings(ini);
  SaveInputSettings(ini);
  SaveFifoPlayerSettings(ini);
  SaveVRSettings(ini);
  SaveAnalyticsSettings(ini);
  SaveNetworkSettings(ini);
  SaveBluetoothPassthroughSettings(ini);
  SaveUSBPassthroughSettings(ini);

  ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));

  Config::Save();
}

void SConfig::SaveSingleSetting(std::string section_name, std::string setting_name,
                                float value_to_save)
{
  IniFile iniFile;
  iniFile.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");

  IniFile::Section* vr = iniFile.GetOrCreateSection(section_name);
  vr->Set(setting_name, value_to_save);
  iniFile.Save(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
}

namespace
{
void CreateDumpPath(const std::string& path)
{
  if (path.empty())
    return;
  File::SetUserPath(D_DUMP_IDX, path + '/');
  File::CreateFullPath(File::GetUserPath(D_DUMPAUDIO_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPSSL_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPFRAMES_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
}
}  // namespace

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
  general->Set("DumpPath", m_DumpPath);
  CreateDumpPath(m_DumpPath);
  general->Set("WirelessMac", m_WirelessMac);
  general->Set("WiiSDCardPath", m_strWiiSDCardPath);

#ifdef USE_GDBSTUB
#ifndef _WIN32
  general->Set("GDBSocket", gdb_socket);
#endif
  general->Set("GDBPort", iGDBPort);
#endif
}

void SConfig::SaveInterfaceSettings(IniFile& ini)
{
  IniFile::Section* interface = ini.GetOrCreateSection("Interface");

  interface->Set("ConfirmStop", bConfirmStop);
  interface->Set("UsePanicHandlers", bUsePanicHandlers);
  interface->Set("OnScreenDisplayMessages", bOnScreenDisplayMessages);
  interface->Set("HideCursor", bHideCursor);
  interface->Set("MainWindowPosX", iPosX);
  interface->Set("MainWindowPosY", iPosY);
  interface->Set("MainWindowWidth", iWidth);
  interface->Set("MainWindowHeight", iHeight);
  interface->Set("LanguageCode", m_InterfaceLanguage);
  interface->Set("ShowToolbar", m_InterfaceToolbar);
  interface->Set("ShowStatusbar", m_InterfaceStatusbar);
  interface->Set("ShowLogWindow", m_InterfaceLogWindow);
  interface->Set("ShowLogConfigWindow", m_InterfaceLogConfigWindow);
  interface->Set("ExtendedFPSInfo", m_InterfaceExtendedFPSInfo);
  interface->Set("ShowActiveTitle", m_show_active_title);
  interface->Set("UseBuiltinTitleDatabase", m_use_builtin_title_database);
  interface->Set("ShowDevelopmentWarning", m_show_development_warning);
  interface->Set("ThemeName", theme_name);
  interface->Set("PauseOnFocusLost", m_PauseOnFocusLost);
  interface->Set("DisableTooltips", m_DisableTooltips);
}

void SConfig::SaveHotkeySettings(IniFile& ini)
{
  IniFile::Section* hotkeys = ini.GetOrCreateSection("Hotkeys");

  hotkeys->Set("XInputPolling", bHotkeysXInput);

  // for (int i = 0; i < NUM_HOTKEYS; i++)
  //{
  //	hotkeys->Set(g_HKData[i].IniText, iHotkey[i]);
  //	hotkeys->Set(std::string(g_HKData[i].IniText) + "Modifier",
  //		iHotkeyModifier[i]);
  //	hotkeys->Set(std::string(g_HKData[i].IniText) + "KBM",
  //		bHotkeyKBM[i]);
  //	hotkeys->Set(std::string(g_HKData[i].IniText) + "DInput",
  //		bHotkeyDInput[i]);
  //	hotkeys->Set(std::string(g_HKData[i].IniText) + "XInputMapping",
  //		iHotkeyDandXInputMapping[i]);
  //	hotkeys->Set(std::string(g_HKData[i].IniText) + "DInputMappingExtra",
  //		iHotkeyDInputMappingExtra[i]);
  //}
}

void SConfig::SaveVRSettings(IniFile& ini)
{
  IniFile::Section* vr = ini.GetOrCreateSection("VR");
  vr->Set("OriginalPrimitiveCount", m_OriginalPrimitiveCount);
  vr->Set("BruteforceScreenshotAll", m_BruteforceScreenshotAll);
}

void SConfig::SaveDisplaySettings(IniFile& ini)
{
  IniFile::Section* display = ini.GetOrCreateSection("Display");

  if (!m_special_case)
    display->Set("FullscreenDisplayRes", strFullscreenResolution);
  display->Set("Fullscreen", bFullscreen);
  display->Set("RenderToMain", bRenderToMain);
  if (!m_special_case)
  {
    display->Set("RenderWindowXPos", iRenderWindowXPos);
    display->Set("RenderWindowYPos", iRenderWindowYPos);
    display->Set("RenderWindowWidth", iRenderWindowWidth);
    display->Set("RenderWindowHeight", iRenderWindowHeight);
  }
  display->Set("RenderWindowAutoSize", bRenderWindowAutoSize);
  display->Set("KeepWindowOnTop", bKeepWindowOnTop);
  display->Set("DisableScreenSaver", bDisableScreenSaver);

#ifdef OCULUSSDK042
  IniFile::Section* vr = ini.GetOrCreateSection("VR");
  vr->Set("AsynchronousTimewarp", bAsynchronousTimewarp);
#endif
}

void SConfig::SaveGameListSettings(IniFile& ini)
{
  IniFile::Section* gamelist = ini.GetOrCreateSection("GameList");

  gamelist->Set("ListDrives", m_ListDrives);
  gamelist->Set("ListWad", m_ListWad);
  gamelist->Set("ListElfDol", m_ListElfDol);
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

  gamelist->Set("ColumnPlatform", m_showSystemColumn);
  gamelist->Set("ColumnBanner", m_showBannerColumn);
  gamelist->Set("ColumnDescription", m_showDescriptionColumn);
  gamelist->Set("ColumnTitle", m_showTitleColumn);
  gamelist->Set("ColumnNotes", m_showMakerColumn);
  gamelist->Set("ColumnFileName", m_showFileNameColumn);
  gamelist->Set("ColumnID", m_showIDColumn);
  gamelist->Set("ColumnRegion", m_showRegionColumn);
  gamelist->Set("ColumnSize", m_showSizeColumn);
  gamelist->Set("ColumnState", m_showStateColumn);
  gamelist->Set("ColumnVRState", m_showVRStateColumn);
}

void SConfig::SaveCoreSettings(IniFile& ini)
{
  IniFile::Section* core = ini.GetOrCreateSection("Core");

  core->Set("SkipIPL", bHLE_BS2);
  core->Set("TimingVariance", iTimingVariance);
  core->Set("CPUCore", iCPUCore);
  core->Set("Fastmem", bFastmem);
  core->Set("CPUThread", bCPUThread);
  core->Set("DSPHLE", bDSPHLE);
  core->Set("SkipIdle", bSkipIdle);
  core->Set("SyncOnSkipIdle", bSyncGPUOnSkipIdleHack);
  core->Set("SyncGPU", bSyncGPU);
  core->Set("SyncGpuMaxDistance", iSyncGpuMaxDistance);
  core->Set("SyncGpuMinDistance", iSyncGpuMinDistance);
  core->Set("SyncGpuOverclock", fSyncGpuOverclock);
  core->Set("FPRF", bFPRF);
  core->Set("AccurateNaNs", bAccurateNaNs);
  core->Set("DefaultISO", m_strDefaultISO);
  core->Set("EnableCheats", bEnableCheats);
  core->Set("SelectedLanguage", SelectedLanguage);
  core->Set("OverrideGCLang", bOverrideGCLanguage);
  core->Set("DPL2Decoder", bDPL2Decoder);
  core->Set("AudioLatency", iLatency);
  core->Set("AudioStretch", m_audio_stretch);
  core->Set("AudioStretchMaxLatency", m_audio_stretch_max_latency);
  core->Set("MemcardAPath", m_strMemoryCardA);
  core->Set("MemcardBPath", m_strMemoryCardB);
  core->Set("AgpCartAPath", m_strGbaCartA);
  core->Set("AgpCartBPath", m_strGbaCartB);
  core->Set("SlotA", m_EXIDevice[0]);
  core->Set("SlotB", m_EXIDevice[1]);
  core->Set("SerialPort1", m_EXIDevice[2]);
  core->Set("BBA_MAC", m_bba_mac);
  for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    core->Set(StringFromFormat("SIDevice%i", i), m_SIDevice[i]);
    core->Set(StringFromFormat("AdapterRumble%i", i), m_AdapterRumble[i]);
    core->Set(StringFromFormat("SimulateKonga%i", i), m_AdapterKonga[i]);
  }
  core->Set("WiiSDCard", m_WiiSDCard);
  core->Set("WiiKeyboard", m_WiiKeyboard);
  core->Set("WiimoteContinuousScanning", m_WiimoteContinuousScanning);
  core->Set("WiimoteEnableSpeaker", m_WiimoteEnableSpeaker);
  core->Set("RunCompareServer", bRunCompareServer);
  core->Set("RunCompareClient", bRunCompareClient);
  // if (!ARBruteForcer::ch_dont_save_settings)
  // 	core->Set("FrameLimit", m_Framelimit);
  core->Set("FrameSkip", m_FrameSkip);
  core->Set("Overclock", m_OCFactor);
  core->Set("OverclockEnable", m_OCEnable);
  core->Set("GFXBackend", m_strVideoBackend);
  core->Set("GPUDeterminismMode", m_strGPUDeterminismMode);
  core->Set("PerfMapDir", m_perfDir);
  core->Set("EnableCustomRTC", bEnableCustomRTC);
  core->Set("CustomRTCValue", m_customRTCValue);
  core->Set("EnableSignatureChecks", m_enable_signature_checks);
}

void SConfig::SaveMovieSettings(IniFile& ini)
{
  IniFile::Section* movie = ini.GetOrCreateSection("Movie");

  movie->Set("PauseMovie", m_PauseMovie);
  movie->Set("Author", m_strMovieAuthor);
  movie->Set("DumpFrames", m_DumpFrames);
  movie->Set("DumpFramesSilent", m_DumpFramesSilent);
  movie->Set("ShowInputDisplay", m_ShowInputDisplay);
  movie->Set("ShowRTC", m_ShowRTC);
}

void SConfig::SaveDSPSettings(IniFile& ini)
{
  IniFile::Section* dsp = ini.GetOrCreateSection("DSP");

  dsp->Set("EnableJIT", m_DSPEnableJIT);
  dsp->Set("DumpAudio", m_DumpAudio);
  dsp->Set("DumpAudioSilent", m_DumpAudioSilent);
  dsp->Set("DumpUCode", m_DumpUCode);
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

  fifoplayer->Set("LoopReplay", bLoopFifoReplay);
}

void SConfig::SaveNetworkSettings(IniFile& ini)
{
  IniFile::Section* network = ini.GetOrCreateSection("Network");

  network->Set("SSLDumpRead", m_SSLDumpRead);
  network->Set("SSLDumpWrite", m_SSLDumpWrite);
  network->Set("SSLVerifyCertificates", m_SSLVerifyCert);
  network->Set("SSLDumpRootCA", m_SSLDumpRootCA);
  network->Set("SSLDumpPeerCert", m_SSLDumpPeerCert);
}

void SConfig::SaveAnalyticsSettings(IniFile& ini)
{
  IniFile::Section* analytics = ini.GetOrCreateSection("Analytics");

  analytics->Set("ID", m_analytics_id);
  analytics->Set("Enabled", m_analytics_enabled);
  analytics->Set("PermissionAsked", m_analytics_permission_asked);
}

void SConfig::SaveBluetoothPassthroughSettings(IniFile& ini)
{
  IniFile::Section* section = ini.GetOrCreateSection("BluetoothPassthrough");

  section->Set("Enabled", m_bt_passthrough_enabled);
  section->Set("VID", m_bt_passthrough_vid);
  section->Set("PID", m_bt_passthrough_pid);
  section->Set("LinkKeys", m_bt_passthrough_link_keys);
}

void SConfig::SaveUSBPassthroughSettings(IniFile& ini)
{
  IniFile::Section* section = ini.GetOrCreateSection("USBPassthrough");

  std::ostringstream oss;
  for (const auto& device : m_usb_passthrough_devices)
    oss << StringFromFormat("%04x:%04x", device.first, device.second) << ',';
  std::string devices_string = oss.str();
  if (!devices_string.empty())
    devices_string.pop_back();

  section->Set("Devices", devices_string);
}

void SConfig::LoadSettings()
{
  Config::Load();

  INFO_LOG(BOOT, "Loading Settings from %s", File::GetUserPath(F_DOLPHINCONFIG_IDX).c_str());
  IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));

  LoadGeneralSettings(ini);
  LoadInterfaceSettings(ini);
  LoadDisplaySettings(ini);
  LoadGameListSettings(ini);
  LoadCoreSettings(ini);
  LoadMovieSettings(ini);
  LoadDSPSettings(ini);
  LoadInputSettings(ini);
  LoadFifoPlayerSettings(ini);
  LoadNetworkSettings(ini);
  LoadVRSettings(ini);
  LoadAnalyticsSettings(ini);
  LoadBluetoothPassthroughSettings(ini);
  LoadUSBPassthroughSettings(ini);
}

void SConfig::LoadGeneralSettings(IniFile& ini)
{
  IniFile::Section* general = ini.GetOrCreateSection("General");

  general->Get("LastFilename", &m_LastFilename);
  general->Get("ShowLag", &m_ShowLag, false);
  general->Get("ShowFrameCount", &m_ShowFrameCount, false);
#ifdef USE_GDBSTUB
#ifndef _WIN32
  general->Get("GDBSocket", &gdb_socket, "");
#endif
  general->Get("GDBPort", &(iGDBPort), -1);
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
  general->Get("DumpPath", &m_DumpPath);
  CreateDumpPath(m_DumpPath);
  general->Get("WirelessMac", &m_WirelessMac);
  general->Get("WiiSDCardPath", &m_strWiiSDCardPath, File::GetUserPath(F_WIISDCARD_IDX));
  File::SetUserPath(F_WIISDCARD_IDX, m_strWiiSDCardPath);
}

void SConfig::LoadInterfaceSettings(IniFile& ini)
{
  IniFile::Section* interface = ini.GetOrCreateSection("Interface");

  interface->Get("ConfirmStop", &bConfirmStop, true);
  interface->Get("UsePanicHandlers", &bUsePanicHandlers, true);
  interface->Get("OnScreenDisplayMessages", &bOnScreenDisplayMessages, true);
  interface->Get("HideCursor", &bHideCursor, false);
  interface->Get("MainWindowPosX", &iPosX, INT_MIN);
  interface->Get("MainWindowPosY", &iPosY, INT_MIN);
  interface->Get("MainWindowWidth", &iWidth, -1);
  interface->Get("MainWindowHeight", &iHeight, -1);
  interface->Get("LanguageCode", &m_InterfaceLanguage, "");
  interface->Get("ShowToolbar", &m_InterfaceToolbar, true);
  interface->Get("ShowStatusbar", &m_InterfaceStatusbar, true);
  interface->Get("ShowLogWindow", &m_InterfaceLogWindow, false);
  interface->Get("ShowLogConfigWindow", &m_InterfaceLogConfigWindow, false);
  interface->Get("ExtendedFPSInfo", &m_InterfaceExtendedFPSInfo, false);
  interface->Get("ShowActiveTitle", &m_show_active_title, true);
  interface->Get("UseBuiltinTitleDatabase", &m_use_builtin_title_database, true);
  interface->Get("ShowDevelopmentWarning", &m_show_development_warning, true);
  interface->Get("ThemeName", &theme_name, DEFAULT_THEME_DIR);
  interface->Get("PauseOnFocusLost", &m_PauseOnFocusLost, false);
  interface->Get("DisableTooltips", &m_DisableTooltips, false);
}

void SConfig::LoadHotkeySettings(IniFile& ini)
{
  IniFile::Section* hotkeys = ini.GetOrCreateSection("Hotkeys");

  hotkeys->Get("XInputPolling", &bHotkeysXInput, true);

  // for (int i = 0; i < NUM_HOTKEYS; i++)
  //{
  //	hotkeys->Get(g_HKData[i].IniText,
  //	    &iHotkey[i], 0);
  //	hotkeys->Get(std::string(g_HKData[i].IniText) + "Modifier",
  //	    &iHotkeyModifier[i], 0);
  //	hotkeys->Get(std::string(g_HKData[i].IniText) + "KBM",
  //		&bHotkeyKBM[i], g_HKData[i].KBM);
  //	hotkeys->Get(std::string(g_HKData[i].IniText) + "DInput",
  //		&bHotkeyDInput[i], g_HKData[i].DInput);
  //	hotkeys->Get(std::string(g_HKData[i].IniText) + "XInputMapping",
  //		&iHotkeyDandXInputMapping[i], g_HKData[i].DandXInputMapping);
  //	hotkeys->Get(std::string(g_HKData[i].IniText) + "DInputMappingExtra",
  //		&iHotkeyDInputMappingExtra[i], g_HKData[i].DInputMappingExtra);
  //}
}

void SConfig::LoadVRSettings(IniFile& ini)
{
  IniFile::Section* vr = ini.GetOrCreateSection("VR");
  vr->Get("OriginalPrimitiveCount", &m_OriginalPrimitiveCount, -1);
  vr->Get("BruteforceScreenshotAll", &m_BruteforceScreenshotAll, false);
}

void SConfig::LoadDisplaySettings(IniFile& ini)
{
  IniFile::Section* display = ini.GetOrCreateSection("Display");

  if (ARBruteForcer::ch_bruteforce)
  {
    bFullscreen = false;
    strFullscreenResolution = "Auto";
    bRenderToMain = false;
    iRenderWindowXPos = -1;
    iRenderWindowYPos = -1;
    iRenderWindowWidth = 640;
    iRenderWindowHeight = 480;
    bRenderWindowAutoSize = false;
    bKeepWindowOnTop = false;
    bDisableScreenSaver = true;
  }
  else
  {
    display->Get("Fullscreen", &bFullscreen, false);
    display->Get("FullscreenDisplayRes", &strFullscreenResolution, "Auto");
    display->Get("RenderToMain", &bRenderToMain, false);
    display->Get("RenderWindowXPos", &iRenderWindowXPos, -1);
    display->Get("RenderWindowYPos", &iRenderWindowYPos, -1);
    display->Get("RenderWindowWidth", &iRenderWindowWidth, 640);
    display->Get("RenderWindowHeight", &iRenderWindowHeight, 480);
    display->Get("RenderWindowAutoSize", &bRenderWindowAutoSize, false);
    display->Get("KeepWindowOnTop", &bKeepWindowOnTop, false);
    display->Get("DisableScreenSaver", &bDisableScreenSaver, true);
  }

#ifdef OCULUSSDK042
  IniFile::Section* vr = ini.GetOrCreateSection("VR");
  vr->Get("AsynchronousTimewarp", &bAsynchronousTimewarp, false);
#else
  bAsynchronousTimewarp = false;
#endif
}

void SConfig::LoadGameListSettings(IniFile& ini)
{
  IniFile::Section* gamelist = ini.GetOrCreateSection("GameList");

  gamelist->Get("ListDrives", &m_ListDrives, false);
  gamelist->Get("ListWad", &m_ListWad, true);
  gamelist->Get("ListElfDol", &m_ListElfDol, true);
  gamelist->Get("ListWii", &m_ListWii, true);
  gamelist->Get("ListGC", &m_ListGC, true);
  gamelist->Get("ListJap", &m_ListJap, true);
  gamelist->Get("ListPal", &m_ListPal, true);
  gamelist->Get("ListUsa", &m_ListUsa, true);

  gamelist->Get("ListAustralia", &m_ListAustralia, true);
  gamelist->Get("ListFrance", &m_ListFrance, true);
  gamelist->Get("ListGermany", &m_ListGermany, true);
  gamelist->Get("ListItaly", &m_ListItaly, true);
  gamelist->Get("ListKorea", &m_ListKorea, true);
  gamelist->Get("ListNetherlands", &m_ListNetherlands, true);
  gamelist->Get("ListRussia", &m_ListRussia, true);
  gamelist->Get("ListSpain", &m_ListSpain, true);
  gamelist->Get("ListTaiwan", &m_ListTaiwan, true);
  gamelist->Get("ListWorld", &m_ListWorld, true);
  gamelist->Get("ListUnknown", &m_ListUnknown, true);
  gamelist->Get("ListSort", &m_ListSort, 3);
  gamelist->Get("ListSortSecondary", &m_ListSort2, 0);

  // Gamelist columns toggles
  gamelist->Get("ColumnPlatform", &m_showSystemColumn, true);
  gamelist->Get("ColumnDescription", &m_showDescriptionColumn, false);
  gamelist->Get("ColumnBanner", &m_showBannerColumn, true);
  gamelist->Get("ColumnTitle", &m_showTitleColumn, true);
  gamelist->Get("ColumnNotes", &m_showMakerColumn, true);
  gamelist->Get("ColumnFileName", &m_showFileNameColumn, false);
  gamelist->Get("ColumnID", &m_showIDColumn, false);
  gamelist->Get("ColumnRegion", &m_showRegionColumn, true);
  gamelist->Get("ColumnSize", &m_showSizeColumn, true);
  gamelist->Get("ColumnState", &m_showStateColumn, true);
  gamelist->Get("ColumnVRState", &m_showVRStateColumn, true);
}

void SConfig::LoadCoreSettings(IniFile& ini)
{
  IniFile::Section* core = ini.GetOrCreateSection("Core");

  core->Get("SkipIPL", &bHLE_BS2, true);
#ifdef _M_X86
  core->Get("CPUCore", &iCPUCore, PowerPC::CORE_JIT64);
#elif _M_ARM_64
  core->Get("CPUCore", &iCPUCore, PowerPC::CORE_JITARM64);
#else
  core->Get("CPUCore", &iCPUCore, PowerPC::CORE_INTERPRETER);
#endif
  core->Get("Fastmem", &bFastmem, true);
  core->Get("DSPHLE", &bDSPHLE, true);
  core->Get("TimingVariance", &iTimingVariance, 40);
  core->Get("CPUThread", &bCPUThread, true);
  core->Get("SkipIdle", &bSkipIdle, true);
  core->Get("SyncOnSkipIdle", &bSyncGPUOnSkipIdleHack, true);
  core->Get("DefaultISO", &m_strDefaultISO);
  core->Get("EnableCheats", &bEnableCheats, false);
  core->Get("SelectedLanguage", &SelectedLanguage, 0);
  core->Get("OverrideGCLang", &bOverrideGCLanguage, false);
  core->Get("DPL2Decoder", &bDPL2Decoder, false);
  core->Get("AudioLatency", &iLatency, 20);
  core->Get("AudioStretch", &m_audio_stretch, false);
  core->Get("AudioStretchMaxLatency", &m_audio_stretch_max_latency, 80);
  core->Get("MemcardAPath", &m_strMemoryCardA);
  core->Get("MemcardBPath", &m_strMemoryCardB);
  core->Get("AgpCartAPath", &m_strGbaCartA);
  core->Get("AgpCartBPath", &m_strGbaCartB);
  core->Get("SlotA", (int*)&m_EXIDevice[0], ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER);
  core->Get("SlotB", (int*)&m_EXIDevice[1], ExpansionInterface::EXIDEVICE_NONE);
  core->Get("SerialPort1", (int*)&m_EXIDevice[2], ExpansionInterface::EXIDEVICE_NONE);
  core->Get("BBA_MAC", &m_bba_mac);
  for (int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    core->Get(StringFromFormat("SIDevice%i", i), (u32*)&m_SIDevice[i],
              (i == 0) ? SerialInterface::SIDEVICE_GC_CONTROLLER : SerialInterface::SIDEVICE_NONE);
    core->Get(StringFromFormat("AdapterRumble%i", i), &m_AdapterRumble[i], true);
    core->Get(StringFromFormat("SimulateKonga%i", i), &m_AdapterKonga[i], false);
  }
  core->Get("WiiSDCard", &m_WiiSDCard, false);
  core->Get("WiiKeyboard", &m_WiiKeyboard, false);
  core->Get("WiimoteContinuousScanning", &m_WiimoteContinuousScanning, false);
  core->Get("WiimoteEnableSpeaker", &m_WiimoteEnableSpeaker, false);
  core->Get("RunCompareServer", &bRunCompareServer, false);
  core->Get("RunCompareClient", &bRunCompareClient, false);
  core->Get("MMU", &bMMU, false);
  core->Get("BBDumpPort", &iBBDumpPort, -1);
  core->Get("SyncGPU", &bSyncGPU, false);
  core->Get("SyncGpuMaxDistance", &iSyncGpuMaxDistance, 200000);
  core->Get("SyncGpuMinDistance", &iSyncGpuMinDistance, -200000);
  core->Get("SyncGpuOverclock", &fSyncGpuOverclock, 1.0f);
  core->Get("FastDiscSpeed", &bFastDiscSpeed, false);
  core->Get("DCBZ", &bDCBZOFF, false);
  core->Get("LowDCBZHack", &bLowDCBZHack, false);
  // if (ARBruteForcer::ch_bruteforce)
  // 	m_Framelimit = 0;
  // else
  // 	core->Get("FrameLimit",                &m_Framelimit,                                  1); //
  // auto frame limit by default
  core->Get("FPRF", &bFPRF, false);
  core->Get("AccurateNaNs", &bAccurateNaNs, false);
  core->Get("EmulationSpeed", &m_EmulationSpeed, 1.0f);
  core->Get("Overclock", &m_OCFactor, 1.0f);
  core->Get("OverclockEnable", &m_OCEnable, false);
  core->Get("FrameSkip", &m_FrameSkip, 0);
  core->Get("GFXBackend", &m_strVideoBackend, "");
  core->Get("GPUDeterminismMode", &m_strGPUDeterminismMode, "auto");
  m_GPUDeterminismMode = ParseGPUDeterminismMode(m_strGPUDeterminismMode);
  core->Get("PerfMapDir", &m_perfDir, "");
  core->Get("EnableCustomRTC", &bEnableCustomRTC, false);
  // Default to seconds between 1.1.1970 and 1.1.2000
  core->Get("CustomRTCValue", &m_customRTCValue, 946684800);
  core->Get("EnableSignatureChecks", &m_enable_signature_checks, true);
}

void SConfig::LoadMovieSettings(IniFile& ini)
{
  IniFile::Section* movie = ini.GetOrCreateSection("Movie");

  movie->Get("PauseMovie", &m_PauseMovie, false);
  movie->Get("Author", &m_strMovieAuthor, "");
  movie->Get("DumpFrames", &m_DumpFrames, false);
  movie->Get("DumpFramesSilent", &m_DumpFramesSilent, false);
  movie->Get("ShowInputDisplay", &m_ShowInputDisplay, false);
  movie->Get("ShowRTC", &m_ShowRTC, false);
}

void SConfig::LoadDSPSettings(IniFile& ini)
{
  IniFile::Section* dsp = ini.GetOrCreateSection("DSP");

  dsp->Get("EnableJIT", &m_DSPEnableJIT, true);
  dsp->Get("DumpAudio", &m_DumpAudio, false);
  dsp->Get("DumpAudioSilent", &m_DumpAudioSilent, false);
  dsp->Get("DumpUCode", &m_DumpUCode, false);
  dsp->Get("Backend", &sBackend, AudioCommon::GetDefaultSoundBackend());
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

  fifoplayer->Get("LoopReplay", &bLoopFifoReplay, true);
}

void SConfig::LoadNetworkSettings(IniFile& ini)
{
  IniFile::Section* network = ini.GetOrCreateSection("Network");

  network->Get("SSLDumpRead", &m_SSLDumpRead, false);
  network->Get("SSLDumpWrite", &m_SSLDumpWrite, false);
  network->Get("SSLVerifyCertificates", &m_SSLVerifyCert, true);
  network->Get("SSLDumpRootCA", &m_SSLDumpRootCA, false);
  network->Get("SSLDumpPeerCert", &m_SSLDumpPeerCert, false);
}

void SConfig::LoadAnalyticsSettings(IniFile& ini)
{
  IniFile::Section* analytics = ini.GetOrCreateSection("Analytics");

  analytics->Get("ID", &m_analytics_id, "");
  analytics->Get("Enabled", &m_analytics_enabled, false);
  analytics->Get("PermissionAsked", &m_analytics_permission_asked, false);
}

void SConfig::LoadBluetoothPassthroughSettings(IniFile& ini)
{
  IniFile::Section* section = ini.GetOrCreateSection("BluetoothPassthrough");

  section->Get("Enabled", &m_bt_passthrough_enabled, false);
  section->Get("VID", &m_bt_passthrough_vid, -1);
  section->Get("PID", &m_bt_passthrough_pid, -1);
  section->Get("LinkKeys", &m_bt_passthrough_link_keys, "");
}

void SConfig::LoadUSBPassthroughSettings(IniFile& ini)
{
  IniFile::Section* section = ini.GetOrCreateSection("USBPassthrough");
  m_usb_passthrough_devices.clear();
  std::string devices_string;
  section->Get("Devices", &devices_string, "");
  for (const auto& pair : SplitString(devices_string, ','))
  {
    const auto index = pair.find(':');
    if (index == std::string::npos)
      continue;

    const u16 vid = static_cast<u16>(strtol(pair.substr(0, index).c_str(), nullptr, 16));
    const u16 pid = static_cast<u16>(strtol(pair.substr(index + 1).c_str(), nullptr, 16));
    if (vid && pid)
      m_usb_passthrough_devices.emplace(vid, pid);
  }
}

void SConfig::ResetRunningGameMetadata()
{
  SetRunningGameMetadata("00000000", 0, 0, Core::TitleDatabase::TitleType::Other);
}

void SConfig::SetRunningGameMetadata(const DiscIO::Volume& volume,
                                     const DiscIO::Partition& partition)
{
  SetRunningGameMetadata(volume.GetGameID(partition), volume.GetTitleID(partition).value_or(0),
                         volume.GetRevision(partition).value_or(0),
                         Core::TitleDatabase::TitleType::Other);
}

void SConfig::SetRunningGameMetadata(const IOS::ES::TMDReader& tmd)
{
  const u64 tmd_title_id = tmd.GetTitleId();

  // If we're launching a disc game, we want to read the revision from
  // the disc header instead of the TMD. They can differ.
  // (IOS HLE ES calls us with a TMDReader rather than a volume when launching
  // a disc game, because ES has no reason to be accessing the disc directly.)
  if (!DVDInterface::UpdateRunningGameMetadata(tmd_title_id))
  {
    // If not launching a disc game, just read everything from the TMD.
    SetRunningGameMetadata(tmd.GetGameID(), tmd_title_id, tmd.GetTitleVersion(),
                           Core::TitleDatabase::TitleType::Channel);
  }
}

void SConfig::SetRunningGameMetadata(const std::string& game_id, u64 title_id, u16 revision,
                                     Core::TitleDatabase::TitleType type)
{
  const bool was_changed = m_game_id != game_id || m_title_id != title_id || m_revision != revision;
  m_game_id = game_id;
  m_title_id = title_id;
  m_revision = revision;

  if (game_id.length() == 6)
  {
    m_debugger_game_id = game_id;
  }
  else if (title_id != 0)
  {
    m_debugger_game_id =
        StringFromFormat("%08X_%08X", static_cast<u32>(title_id >> 32), static_cast<u32>(title_id));
  }
  else
  {
    m_debugger_game_id.clear();
  }

  if (!was_changed)
    return;

  if (game_id == "00000000")
  {
    m_title_description.clear();
    return;
  }

  const Core::TitleDatabase title_database;
  m_title_description = title_database.Describe(m_game_id, type);
  NOTICE_LOG(CORE, "Active title: %s", m_title_description.c_str());

  Config::CreateVRGameLayer();
  Config::AddLayer(ConfigLoaders::GenerateGlobalGameConfigLoader(game_id, revision));
  Config::AddLayer(ConfigLoaders::GenerateLocalGameConfigLoader(game_id, revision));

  if (Core::IsRunning())
  {
    // TODO: have a callback mechanism for title changes?
    g_symbolDB.Clear();
    CBoot::LoadMapFromFilename();
    HLE::Reload();
    PatchEngine::Reload();
    HiresTexture::Update();
    DolphinAnalytics::Instance()->ReportGameStart();
  }
}

void SConfig::LoadDefaults()
{
  bEnableDebugging = false;
  bAutomaticStart = false;
  bBootToPause = false;

#ifdef USE_GDBSTUB
  iGDBPort = -1;
#ifndef _WIN32
  gdb_socket = "";
#endif
#endif

  iCPUCore = PowerPC::DefaultCPUCore();
  iTimingVariance = 40;
  bCPUThread = false;
  bSkipIdle = true;
  bSyncGPUOnSkipIdleHack = true;
  bRunCompareServer = false;
  bDSPHLE = true;
  bFastmem = true;
  bFPRF = false;
  bAccurateNaNs = false;
  bMMU = false;
  bDCBZOFF = false;
  bLowDCBZHack = false;
  iBBDumpPort = -1;
  bSyncGPU = false;
  bFastDiscSpeed = false;
  m_strWiiSDCardPath = File::GetUserPath(F_WIISDCARD_IDX);
  bEnableMemcardSdWriting = true;
  SelectedLanguage = 0;
  bOverrideGCLanguage = false;
  bWii = false;
  bDPL2Decoder = false;
  iLatency = 20;
  m_audio_stretch = false;
  m_audio_stretch_max_latency = 80;

  iPosX = INT_MIN;
  iPosY = INT_MIN;
  iWidth = -1;
  iHeight = -1;

  m_analytics_id = "";
  m_analytics_enabled = false;
  m_analytics_permission_asked = false;

  bLoopFifoReplay = true;

  bJITOff = false;  // debugger only settings
  bJITLoadStoreOff = false;
  bJITLoadStoreFloatingOff = false;
  bJITLoadStorePairedOff = false;
  bJITFloatingPointOff = false;
  bJITIntegerOff = false;
  bJITPairedOff = false;
  bJITSystemRegistersOff = false;
  bJITBranchOff = false;

  ResetRunningGameMetadata();
}

bool SConfig::IsUSBDeviceWhitelisted(const std::pair<u16, u16> vid_pid) const
{
  return m_usb_passthrough_devices.find(vid_pid) != m_usb_passthrough_devices.end();
}

// The reason we need this function is because some memory card code
// expects to get a non-NTSC-K region even if we're emulating an NTSC-K Wii.
DiscIO::Region SConfig::ToGameCubeRegion(DiscIO::Region region)
{
  if (region != DiscIO::Region::NTSC_K)
    return region;

  // GameCube has no NTSC-K region. No choice of replacement value is completely
  // non-arbitrary, but let's go with NTSC-J since Korean GameCubes are NTSC-J.
  return DiscIO::Region::NTSC_J;
}

const char* SConfig::GetDirectoryForRegion(DiscIO::Region region)
{
  switch (region)
  {
  case DiscIO::Region::NTSC_J:
    return JAP_DIR;

  case DiscIO::Region::NTSC_U:
    return USA_DIR;

  case DiscIO::Region::PAL:
    return EUR_DIR;

  case DiscIO::Region::NTSC_K:
    _assert_msg_(BOOT, false, "NTSC-K is not a valid GameCube region");
    return nullptr;

  default:
    return nullptr;
  }
}

std::string SConfig::GetBootROMPath(const std::string& region_directory) const
{
  const std::string path =
      File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + region_directory + DIR_SEP GC_IPL;
  if (!File::Exists(path))
    return File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + region_directory + DIR_SEP GC_IPL;
  return path;
}

struct SetGameMetadata
{
  SetGameMetadata(SConfig* config_, DiscIO::Region* region_) : config(config_), region(region_) {}
  bool operator()(const BootParameters::Disc& disc) const
  {
    config->SetRunningGameMetadata(*disc.volume, disc.volume->GetGamePartition());
    config->bWii = disc.volume->GetVolumeType() == DiscIO::Platform::WII_DISC;
    config->m_disc_booted_from_game_list = true;
    *region = disc.volume->GetRegion();
    return true;
  }

  bool operator()(const BootParameters::Executable& executable) const
  {
    if (!executable.reader->IsValid())
      return false;

    config->bWii = executable.reader->IsWii();

    *region = DiscIO::Region::UNKNOWN_REGION;

    // Strip the .elf/.dol file extension and directories before the name
    SplitPath(executable.path, nullptr, &config->m_debugger_game_id, nullptr);
    return true;
  }

  bool operator()(const DiscIO::WiiWAD& wad) const
  {
    if (!wad.IsValid() || !wad.GetTMD().IsValid())
    {
      PanicAlertT("This WAD is not valid.");
      return false;
    }
    if (!IOS::ES::IsChannel(wad.GetTMD().GetTitleId()))
    {
      PanicAlertT("This WAD is not bootable.");
      return false;
    }

    const IOS::ES::TMDReader& tmd = wad.GetTMD();
    config->SetRunningGameMetadata(tmd);
    config->bWii = true;
    *region = tmd.GetRegion();
    return true;
  }

  bool operator()(const BootParameters::NANDTitle& nand_title) const
  {
    IOS::HLE::Kernel ios;
    const IOS::ES::TMDReader tmd = ios.GetES()->FindInstalledTMD(nand_title.id);
    if (!tmd.IsValid() || !IOS::ES::IsChannel(nand_title.id))
    {
      PanicAlertT("This title cannot be booted.");
      return false;
    }
    config->SetRunningGameMetadata(tmd);
    config->bWii = true;
	config->m_is_nand = true;
    *region = tmd.GetRegion();
    return true;
  }

  bool operator()(const BootParameters::IPL& ipl) const
  {
    config->bWii = false;
    *region = ipl.region;
    return true;
  }

  bool operator()(const BootParameters::DFF& dff) const
  {
    std::unique_ptr<FifoDataFile> dff_file(FifoDataFile::Load(dff.dff_path, true));
    if (!dff_file)
      return false;

    config->bWii = dff_file->GetIsWii();
    *region = DiscIO::Region::NTSC_U;
    return true;
  }

private:
  SConfig* config;
  DiscIO::Region* region;
};

bool SConfig::SetPathsAndGameMetadata(const BootParameters& boot)
{
  m_is_mios = false;
  m_is_nand = false;
  m_disc_booted_from_game_list = false;
  if (!std::visit(SetGameMetadata(this, &m_region), boot.parameters))
    return false;

  // Fall back to the system menu region, if possible.
  if (m_region == DiscIO::Region::UNKNOWN_REGION)
  {
    IOS::HLE::Kernel ios;
    const IOS::ES::TMDReader system_menu_tmd = ios.GetES()->FindInstalledTMD(Titles::SYSTEM_MENU);
    if (system_menu_tmd.IsValid())
      m_region = system_menu_tmd.GetRegion();
  }

  // Fall back to PAL.
  if (m_region == DiscIO::Region::UNKNOWN_REGION)
    m_region = DiscIO::Region::PAL;

  // Set up paths
  const std::string region_dir = GetDirectoryForRegion(ToGameCubeRegion(m_region));
  CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardA, region_dir, true);
  CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardB, region_dir, false);
  m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
  m_strBootROM = GetBootROMPath(region_dir);

  return true;
}

void SConfig::CheckMemcardPath(std::string& memcardPath, const std::string& gameRegion,
                               bool isSlotA)
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
    std::string region = filename.substr(filename.size() - 7, 3);
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
        filename.replace(filename.size() - 4, 4, ext);
        if (PanicYesNoT("Memory Card filename in Slot %c is incorrect\n"
                        "Region not specified\n\n"
                        "Slot %c path was changed to\n"
                        "%s\n"
                        "Would you like to copy the old file to this new location?\n",
                        isSlotA ? 'A' : 'B', isSlotA ? 'A' : 'B', filename.c_str()))
        {
          if (!File::Copy(oldFilename, filename))
            PanicAlertT("Copy failed");
        }
      }
      memcardPath = filename;  // Always correct the path!
    }
    else if (region.compare(gameRegion) != 0)
    {
      // filename has region, but it's not == gameRegion
      // Just set the correct filename, the EXI Device will create it if it doesn't exist
      memcardPath = filename.replace(filename.size() - ext.size(), ext.size(), ext);
    }
  }
}

DiscIO::Language SConfig::GetCurrentLanguage(bool wii) const
{
  int language_value;
  if (wii)
    language_value = Config::Get(Config::SYSCONF_LANGUAGE);
  else
    language_value = SConfig::GetInstance().SelectedLanguage + 1;
  DiscIO::Language language = static_cast<DiscIO::Language>(language_value);

  // Get rid of invalid values (probably doesn't matter, but might as well do it)
  if (language > DiscIO::Language::LANGUAGE_UNKNOWN ||
      language < DiscIO::Language::LANGUAGE_JAPANESE)
    language = DiscIO::Language::LANGUAGE_UNKNOWN;
  return language;
}

IniFile SConfig::LoadDefaultGameIni() const
{
  return LoadDefaultGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadLocalGameIni() const
{
  return LoadLocalGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadGameIni() const
{
  return LoadGameIni(GetGameID(), m_revision);
}

IniFile SConfig::LoadDefaultGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  return game_ini;
}

IniFile SConfig::LoadLocalGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
  return game_ini;
}

IniFile SConfig::LoadGameIni(const std::string& id, std::optional<u16> revision)
{
  IniFile game_ini;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(id, revision))
    game_ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
  return game_ini;
}
