// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <climits>
#include <memory>

#include "AudioCommon/AudioCommon.h"

#include "Common/CDUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/SysConf.h"

#include "Core/Boot/Boot.h"
#include "Core/Boot/Boot_DOL.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"  // for bWii
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/HW/SI/SI.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/Enums.h"
#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

GPUDeterminismMode SConfig::ParseGPUDeterminismMode(const std::string& mode)
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

SConfig* SConfig::m_Instance;

SConfig::SConfig()
{
  LoadDefaults();
  // Make sure we have log manager
  LoadSettings();
  LoadSettingsFromSysconf();
}

void SConfig::Init()
{
  m_Instance = new SConfig;

  Config::AddConfigChangedCallback([&]() { m_Instance->LoadSettings(); });
}

void SConfig::Shutdown()
{
  Config::Save();
  delete m_Instance;
  m_Instance = nullptr;
}

SConfig::~SConfig()
{
  SaveSettingsToSysconf();
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

void SConfig::SaveSettingsToSysconf()
{
  SysConf sysconf{Common::FromWhichRoot::FROM_CONFIGURED_ROOT};

  sysconf.SetData<u8>("IPL.SSV", m_wii_screensaver);
  sysconf.SetData<u8>("IPL.LNG", m_wii_language);

  sysconf.SetData<u8>("IPL.AR", m_wii_aspect_ratio);
  sysconf.SetData<u8>("BT.BAR", m_sensor_bar_position);
  sysconf.SetData<u32>("BT.SENS", m_sensor_bar_sensitivity);
  sysconf.SetData<u8>("BT.SPKV", m_speaker_volume);
  sysconf.SetData("BT.MOT", m_wiimote_motor);
  sysconf.SetData("IPL.PGS", bProgressive);
  sysconf.SetData("IPL.E60", bPAL60);

  // Disable WiiConnect24's standby mode. If it is enabled, it prevents us from receiving
  // shutdown commands in the State Transition Manager (STM).
  // TODO: remove this if and once Dolphin supports WC24 standby mode.
  sysconf.SetData<u8>("IPL.IDL", 0x00);
  NOTICE_LOG(COMMON, "Disabling WC24 'standby' (shutdown to idle) to avoid hanging on shutdown");

  IOS::HLE::RestoreBTInfoSection(&sysconf);

  sysconf.Save();
}

void SConfig::LoadSettings()
{
  LoadGeneralSettings();
  LoadInterfaceSettings();
  LoadDisplaySettings();
  LoadGameListSettings();
  LoadCoreSettings();
  LoadMovieSettings();
  LoadDSPSettings();
  LoadInputSettings();
  LoadFifoPlayerSettings();
  LoadNetworkSettings();
  LoadAnalyticsSettings();
  LoadBluetoothPassthroughSettings();
  LoadUSBPassthroughSettings();
  LoadSysconfSettings();
}

void SConfig::LoadGeneralSettings()
{
  Config::Section* general = Config::GetOrCreateSection(Config::System::Main, "General");

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

  general->Get("RecursiveISOPaths", &m_RecursiveISOFolder, false);
  general->Get("NANDRootPath", &m_NANDPath);
  File::SetUserPath(D_WIIROOT_IDX, m_NANDPath);
  general->Get("DumpPath", &m_DumpPath);
  CreateDumpPath(m_DumpPath);
  general->Get("WirelessMac", &m_WirelessMac);
  general->Get("WiiSDCardPath", &m_strWiiSDCardPath, File::GetUserPath(F_WIISDCARD_IDX));
  File::SetUserPath(F_WIISDCARD_IDX, m_strWiiSDCardPath);
}

void SConfig::LoadInterfaceSettings()
{
  Config::Section* interface = Config::GetOrCreateSection(Config::System::Main, "Interface");

  interface->Get("ConfirmStop", &bConfirmStop, true);
  interface->Get("UsePanicHandlers", &bUsePanicHandlers, true);
  interface->Get("OnScreenDisplayMessages", &bOnScreenDisplayMessages, true);
  interface->Get("HideCursor", &bHideCursor, false);
  interface->Get("AutoHideCursor", &bAutoHideCursor, false);
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
  interface->Get("ThemeName", &theme_name, DEFAULT_THEME_DIR);
  interface->Get("PauseOnFocusLost", &m_PauseOnFocusLost, false);
  interface->Get("DisableTooltips", &m_DisableTooltips, false);
}

void SConfig::LoadDisplaySettings()
{
  Config::Section* display = Config::GetOrCreateSection(Config::System::Main, "Display");

  display->Get("Fullscreen", &bFullscreen, false);
  display->Get("FullscreenResolution", &strFullscreenResolution, "Auto");
  display->Get("RenderToMain", &bRenderToMain, false);
  display->Get("RenderWindowXPos", &iRenderWindowXPos, -1);
  display->Get("RenderWindowYPos", &iRenderWindowYPos, -1);
  display->Get("RenderWindowWidth", &iRenderWindowWidth, 640);
  display->Get("RenderWindowHeight", &iRenderWindowHeight, 480);
  display->Get("RenderWindowAutoSize", &bRenderWindowAutoSize, false);
  display->Get("KeepWindowOnTop", &bKeepWindowOnTop, false);
  display->Get("ProgressiveScan", &bProgressive, false);
  display->Get("PAL60", &bPAL60, true);
  display->Get("DisableScreenSaver", &bDisableScreenSaver, true);
  display->Get("ForceNTSCJ", &bForceNTSCJ, false);
}

void SConfig::LoadGameListSettings()
{
  Config::Section* gamelist = Config::GetOrCreateSection(Config::System::Main, "GameList");

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

  // Determines if compressed games display in blue
  gamelist->Get("ColorCompressed", &m_ColorCompressed, true);

  // Gamelist columns toggles
  gamelist->Get("ColumnPlatform", &m_showSystemColumn, true);
  gamelist->Get("ColumnBanner", &m_showBannerColumn, true);
  gamelist->Get("ColumnNotes", &m_showMakerColumn, true);
  gamelist->Get("ColumnFileName", &m_showFileNameColumn, false);
  gamelist->Get("ColumnID", &m_showIDColumn, false);
  gamelist->Get("ColumnRegion", &m_showRegionColumn, true);
  gamelist->Get("ColumnSize", &m_showSizeColumn, true);
  gamelist->Get("ColumnState", &m_showStateColumn, true);
}

void SConfig::LoadCoreSettings()
{
  Config::Section* core = Config::GetOrCreateSection(Config::System::Main, "Core");

  core->Get("HLE_BS2", &bHLE_BS2, false);
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
  core->Get("SyncOnSkipIdle", &bSyncGPUOnSkipIdleHack, true);
  core->Get("DefaultISO", &m_strDefaultISO);
  core->Get("DVDRoot", &m_strDVDRoot);
  core->Get("Apploader", &m_strApploader);
  core->Get("EnableCheats", &bEnableCheats, false);
  core->Get("SelectedLanguage", &SelectedLanguage, 0);
  core->Get("OverrideGCLang", &bOverrideGCLanguage, false);
  core->Get("DPL2Decoder", &bDPL2Decoder, false);
  core->Get("Latency", &iLatency, 2);
  core->Get("MemcardAPath", &m_strMemoryCardA);
  core->Get("MemcardBPath", &m_strMemoryCardB);
  core->Get("AgpCartAPath", &m_strGbaCartA);
  core->Get("AgpCartBPath", &m_strGbaCartB);
  core->Get("SlotA", (int*)&m_EXIDevice[0], EXIDEVICE_MEMORYCARD);
  core->Get("SlotB", (int*)&m_EXIDevice[1], EXIDEVICE_NONE);
  core->Get("SerialPort1", (int*)&m_EXIDevice[2], EXIDEVICE_NONE);
  core->Get("EnableSaving", &bEnableMemcardSdWriting, true);
  core->Get("BBA_MAC", &m_bba_mac);
  core->Get("TimeProfiling", &bJITILTimeProfiling, false);
  core->Get("OutputIR", &bJITILOutputIR, false);
  for (int i = 0; i < MAX_SI_CHANNELS; ++i)
  {
    core->Get(StringFromFormat("SIDevice%i", i), (u32*)&m_SIDevice[i],
              (i == 0) ? SIDEVICE_GC_CONTROLLER : SIDEVICE_NONE);
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
  core->Get("FPRF", &bFPRF, false);
  core->Get("AccurateNaNs", &bAccurateNaNs, false);
  core->Get("EmulationSpeed", &m_EmulationSpeed, 1.0f);
  core->Get("Overclock", &m_OCFactor, 1.0f);
  core->Get("OverclockEnable", &m_OCEnable, false);
  core->Get("FrameSkip", &m_FrameSkip, 0);
  core->Get("GFXBackend", &m_strVideoBackend, "");
  core->Get("GPUDeterminismMode", &m_strGPUDeterminismMode, "auto");
  core->Get("PerfMapDir", &m_perfDir, "");
  core->Get("EnableCustomRTC", &bEnableCustomRTC, false);
  // Default to seconds between 1.1.1970 and 1.1.2000
  core->Get("CustomRTCValue", &m_customRTCValue, 946684800);

  m_GPUDeterminismMode = ParseGPUDeterminismMode(m_strGPUDeterminismMode);
}

void SConfig::LoadMovieSettings()
{
  Config::Section* movie = Config::GetOrCreateSection(Config::System::Main, "Movie");

  movie->Get("PauseMovie", &m_PauseMovie, false);
  movie->Get("Author", &m_strMovieAuthor, "");
  movie->Get("DumpFrames", &m_DumpFrames, false);
  movie->Get("DumpFramesSilent", &m_DumpFramesSilent, false);
  movie->Get("ShowInputDisplay", &m_ShowInputDisplay, false);
  movie->Get("ShowRTC", &m_ShowRTC, false);
}

void SConfig::LoadDSPSettings()
{
  Config::Section* dsp = Config::GetOrCreateSection(Config::System::Main, "DSP");

  dsp->Get("EnableJIT", &m_DSPEnableJIT, true);
  dsp->Get("DumpAudio", &m_DumpAudio, false);
  dsp->Get("DumpAudioSilent", &m_DumpAudioSilent, false);
  dsp->Get("DumpUCode", &m_DumpUCode, false);
  dsp->Get("Backend", &sBackend, AudioCommon::GetDefaultSoundBackend());
  dsp->Get("Volume", &m_Volume, 100);
  dsp->Get("CaptureLog", &m_DSPCaptureLog, false);

  m_IsMuted = false;
}

void SConfig::LoadInputSettings()
{
  Config::Section* input = Config::GetOrCreateSection(Config::System::Main, "Input");

  input->Get("BackgroundInput", &m_BackgroundInput, false);
}

void SConfig::LoadFifoPlayerSettings()
{
  Config::Section* fifoplayer = Config::GetOrCreateSection(Config::System::Main, "FifoPlayer");

  fifoplayer->Get("LoopReplay", &bLoopFifoReplay, true);
}

void SConfig::LoadNetworkSettings()
{
  auto* network = Config::GetOrCreateSection(Config::System::Main, "Network");

  network->Get("SSLDumpRead", &m_SSLDumpRead, false);
  network->Get("SSLDumpWrite", &m_SSLDumpWrite, false);
  network->Get("SSLVerifyCert", &m_SSLVerifyCert, false);
  network->Get("SSLDumpRootCA", &m_SSLDumpRootCA, false);
  network->Get("SSLDumpPeerCert", &m_SSLDumpPeerCert, false);
}

void SConfig::LoadAnalyticsSettings()
{
  Config::Section* analytics = Config::GetOrCreateSection(Config::System::Main, "Analytics");

  analytics->Get("ID", &m_analytics_id, "");
  analytics->Get("Enabled", &m_analytics_enabled, false);
  analytics->Get("PermissionAsked", &m_analytics_permission_asked, false);
}

void SConfig::LoadBluetoothPassthroughSettings()
{
  auto* section = Config::GetOrCreateSection(Config::System::Main, "BluetoothPassthrough");

  section->Get("Enabled", &m_bt_passthrough_enabled, false);
  section->Get("VID", &m_bt_passthrough_vid, -1);
  section->Get("PID", &m_bt_passthrough_pid, -1);
  section->Get("LinkKeys", &m_bt_passthrough_link_keys, "");
}

void SConfig::LoadUSBPassthroughSettings()
{
  auto* section = Config::GetOrCreateSection(Config::System::Main, "USBPassthrough");

  m_usb_passthrough_devices.clear();
  std::string devices_string;
  std::vector<std::string> pairs;
  section->Get("Devices", &devices_string, "");
  SplitString(devices_string, ',', pairs);
  for (const auto& pair : pairs)
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

void SConfig::LoadSysconfSettings()
{
  auto* section = Config::GetOrCreateSection(Config::System::Main, "Sysconf");

  section->Get("SensorBarPosition", &m_sensor_bar_position, m_sensor_bar_position);
  section->Get("SensorBarSensitivity", &m_sensor_bar_sensitivity, m_sensor_bar_sensitivity);
  section->Get("SpeakerVolume", &m_speaker_volume, m_speaker_volume);
  section->Get("WiimoteMotor", &m_wiimote_motor, m_wiimote_motor);
  section->Get("WiiLanguage", &m_wii_language, m_wii_language);
  section->Get("AspectRatio", &m_wii_aspect_ratio, m_wii_aspect_ratio);
  section->Get("Screensaver", &m_wii_screensaver, m_wii_screensaver);
}

void SConfig::LoadSettingsFromSysconf()
{
  SysConf sysconf{Common::FromWhichRoot::FROM_CONFIGURED_ROOT};

  m_wii_screensaver = sysconf.GetData<u8>("IPL.SSV");
  m_wii_language = sysconf.GetData<u8>("IPL.LNG");
  m_wii_aspect_ratio = sysconf.GetData<u8>("IPL.AR");
  m_sensor_bar_position = sysconf.GetData<u8>("BT.BAR");
  m_sensor_bar_sensitivity = sysconf.GetData<u32>("BT.SENS");
  m_speaker_volume = sysconf.GetData<u8>("BT.SPKV");
  m_wiimote_motor = sysconf.GetData<u8>("BT.MOT") != 0;
  bProgressive = sysconf.GetData<u8>("IPL.PGS") != 0;
  bPAL60 = sysconf.GetData<u8>("IPL.E60") != 0;
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

  iCPUCore = PowerPC::CORE_JIT64;
  iTimingVariance = 40;
  bCPUThread = false;
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
  iLatency = 14;

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

  m_strName = "NONE";
  m_strGameID = "00000000";
  m_title_id = 0;
  m_revision = 0;
}

bool SConfig::IsUSBDeviceWhitelisted(const std::pair<u16, u16> vid_pid) const
{
  return m_usb_passthrough_devices.find(vid_pid) != m_usb_passthrough_devices.end();
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
    // This function can't return a Korean directory name, because this
    // function is only used for GameCube things (memory cards, IPL), and
    // GameCube has no NTSC-K region. Since NTSC-K doesn't correspond to any
    // GameCube region, let's return an arbitrary pick. Returning nullptr like
    // with unknown regions would be inappropriate, because Dolphin expects
    // to get valid memory card paths even when running an NTSC-K Wii game.
    return JAP_DIR;

  default:
    return nullptr;
  }
}

// Sets m_region to the region parameter, or to PAL if the region parameter
// is invalid. Set directory_name to the value returned by GetDirectoryForRegion
// for m_region. Returns false if the region parameter is invalid.
bool SConfig::SetRegion(DiscIO::Region region, std::string* directory_name)
{
  const char* retrieved_region_dir = GetDirectoryForRegion(region);
  m_region = retrieved_region_dir ? region : DiscIO::Region::PAL;
  *directory_name = retrieved_region_dir ? retrieved_region_dir : EUR_DIR;
  return !!retrieved_region_dir;
}

bool SConfig::AutoSetup(EBootBS2 _BootBS2)
{
  std::string set_region_dir(EUR_DIR);

  switch (_BootBS2)
  {
  case BOOT_DEFAULT:
  {
    bool bootDrive = cdio_is_cdrom(m_strFilename);
    // Check if the file exist, we may have gotten it from a --elf command line
    // that gave an incorrect file name
    if (!bootDrive && !File::Exists(m_strFilename))
    {
      PanicAlertT("The specified file \"%s\" does not exist", m_strFilename.c_str());
      return false;
    }

    std::string Extension;
    SplitPath(m_strFilename, nullptr, nullptr, &Extension);
    if (!strcasecmp(Extension.c_str(), ".gcm") || !strcasecmp(Extension.c_str(), ".iso") ||
        !strcasecmp(Extension.c_str(), ".tgc") || !strcasecmp(Extension.c_str(), ".wbfs") ||
        !strcasecmp(Extension.c_str(), ".ciso") || !strcasecmp(Extension.c_str(), ".gcz") ||
        bootDrive)
    {
      m_BootType = BOOT_ISO;
      std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(m_strFilename));
      if (pVolume == nullptr)
      {
        if (bootDrive)
          PanicAlertT("Could not read \"%s\". "
                      "There is no disc in the drive or it is not a GameCube/Wii backup. "
                      "Please note that Dolphin cannot play games directly from the original "
                      "GameCube and Wii discs.",
                      m_strFilename.c_str());
        else
          PanicAlertT("\"%s\" is an invalid GCM/ISO file, or is not a GC/Wii ISO.",
                      m_strFilename.c_str());
        return false;
      }
      m_strName = pVolume->GetInternalName();
      m_strGameID = pVolume->GetGameID();
      pVolume->GetTitleID(&m_title_id);
      m_revision = pVolume->GetRevision();

      // Check if we have a Wii disc
      bWii = pVolume->GetVolumeType() == DiscIO::Platform::WII_DISC;

      if (!SetRegion(pVolume->GetRegion(), &set_region_dir))
        if (!PanicYesNoT("Your GCM/ISO file seems to be invalid (invalid country)."
                         "\nContinue with PAL region?"))
          return false;
    }
    else if (!strcasecmp(Extension.c_str(), ".elf"))
    {
      bWii = CBoot::IsElfWii(m_strFilename);
      // TODO: Right now GC homebrew boots in NTSC and Wii homebrew in PAL.
      // This is intentional so that Wii homebrew can boot in both 50Hz and
      // 60Hz, without forcing
      // all GC homebrew to 50Hz.
      // In the future, it probably makes sense to add a Region setting for
      // homebrew somewhere in
      // the emulator config.
      SetRegion(bWii ? DiscIO::Region::PAL : DiscIO::Region::NTSC_U, &set_region_dir);
      m_BootType = BOOT_ELF;
    }
    else if (!strcasecmp(Extension.c_str(), ".dol"))
    {
      CDolLoader dolfile(m_strFilename);
      bWii = dolfile.IsWii();
      // TODO: See the ELF code above.
      SetRegion(bWii ? DiscIO::Region::PAL : DiscIO::Region::NTSC_U, &set_region_dir);
      m_BootType = BOOT_DOL;
    }
    else if (!strcasecmp(Extension.c_str(), ".dff"))
    {
      bWii = true;
      SetRegion(DiscIO::Region::NTSC_U, &set_region_dir);
      m_BootType = BOOT_DFF;

      std::unique_ptr<FifoDataFile> ddfFile(FifoDataFile::Load(m_strFilename, true));

      if (ddfFile)
      {
        bWii = ddfFile->GetIsWii();
      }
    }
    else if (DiscIO::CNANDContentManager::Access().GetNANDLoader(m_strFilename).IsValid())
    {
      std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(m_strFilename));
      const DiscIO::CNANDContentLoader& ContentLoader =
          DiscIO::CNANDContentManager::Access().GetNANDLoader(m_strFilename);

      if (ContentLoader.GetContentByIndex(ContentLoader.GetBootIndex()) == nullptr)
      {
        // WAD is valid yet cannot be booted. Install instead.
        u64 installed = DiscIO::CNANDContentManager::Access().Install_WiiWAD(m_strFilename);
        if (installed)
          SuccessAlertT("The WAD has been installed successfully");
        return false;  // do not boot
      }

      SetRegion(ContentLoader.GetRegion(), &set_region_dir);

      bWii = true;
      m_BootType = BOOT_WII_NAND;

      if (pVolume)
      {
        m_strName = pVolume->GetInternalName();
        m_strGameID = pVolume->GetGameID();
        pVolume->GetTitleID(&m_title_id);
      }
      else
      {
        // null pVolume means that we are loading from nand folder (Most Likely
        // Wii Menu)
        // if this is the second boot we would be using the Name and id of the
        // last title
        m_strName.clear();
        m_strGameID.clear();
        m_title_id = 0;
      }

      // Use the TitleIDhex for name and/or game ID if launching
      // from nand folder or if it is not ascii characters
      // (specifically sysmenu could potentially apply to other things)
      std::string titleidstr = StringFromFormat("%016" PRIx64, m_title_id);

      if (m_strName.empty())
      {
        m_strName = titleidstr;
      }
      if (m_strGameID.empty())
      {
        m_strGameID = titleidstr;
      }
    }
    else
    {
      PanicAlertT("Could not recognize ISO file %s", m_strFilename.c_str());
      return false;
    }
  }
  break;

  case BOOT_BS2_USA:
    SetRegion(DiscIO::Region::NTSC_U, &set_region_dir);
    m_strFilename.clear();
    break;

  case BOOT_BS2_JAP:
    SetRegion(DiscIO::Region::NTSC_J, &set_region_dir);
    m_strFilename.clear();
    break;

  case BOOT_BS2_EUR:
    SetRegion(DiscIO::Region::PAL, &set_region_dir);
    m_strFilename.clear();
    break;
  }

  // Setup paths
  CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardA, set_region_dir, true);
  CheckMemcardPath(SConfig::GetInstance().m_strMemoryCardB, set_region_dir, false);
  m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
  if (!bWii)
  {
    if (!bHLE_BS2)
    {
      m_strBootROM = File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + set_region_dir + DIR_SEP GC_IPL;
      if (!File::Exists(m_strBootROM))
        m_strBootROM =
            File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + set_region_dir + DIR_SEP GC_IPL;

      if (!File::Exists(m_strBootROM))
      {
        WARN_LOG(BOOT, "Bootrom file %s not found - using HLE.", m_strBootROM.c_str());
        bHLE_BS2 = true;
      }
    }
  }
  else if (bWii && !bHLE_BS2)
  {
    WARN_LOG(BOOT, "GC bootrom file will not be loaded for Wii mode.");
    bHLE_BS2 = true;
  }

  Config::AddLoadLayer(GenerateGlobalGameConfigLoader(GetGameID(), m_revision));
  Config::AddLoadLayer(GenerateLocalGameConfigLoader(GetGameID(), m_revision));

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
      // Just set the correct filename, the EXI Device will create it if it
      // doesn't exist
      memcardPath = filename.replace(filename.size() - ext.size(), ext.size(), ext);
    }
  }
}

DiscIO::Language SConfig::GetCurrentLanguage(bool wii) const
{
  int language_value;
  if (wii)
    language_value = SConfig::GetInstance().m_wii_language;
  else
    language_value = SConfig::GetInstance().SelectedLanguage + 1;
  DiscIO::Language language = static_cast<DiscIO::Language>(language_value);

  // Get rid of invalid values (probably doesn't matter, but might as well do it)
  if (language > DiscIO::Language::LANGUAGE_UNKNOWN ||
      language < DiscIO::Language::LANGUAGE_JAPANESE)
    language = DiscIO::Language::LANGUAGE_UNKNOWN;
  return language;
}

// Returns all possible filenames in ascending order of priority
std::vector<std::string> SConfig::GetGameIniFilenames(const std::string& id, u16 revision)
{
  std::vector<std::string> filenames;

  if (id.empty())
    return filenames;

  // INIs that match the system code (unique for each Virtual Console system)
  filenames.push_back(id.substr(0, 1) + ".ini");

  // INIs that match all regions
  if (id.size() >= 4)
    filenames.push_back(id.substr(0, 3) + ".ini");

  // Regular INIs
  filenames.push_back(id + ".ini");

  // INIs with specific revisions
  filenames.push_back(id + StringFromFormat("r%d", revision) + ".ini");

  return filenames;
}
