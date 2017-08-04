// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Common/IniFile.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/TitleDatabase.h"

namespace DiscIO
{
enum class Language;
enum class Region;
struct Partition;
class Volume;
}
namespace IOS
{
namespace ES
{
class TMDReader;
}
}

// DSP Backend Types
#define BACKEND_NULLSOUND _trans("No audio output")
#define BACKEND_ALSA "ALSA"
#define BACKEND_CUBEB "Cubeb"
#define BACKEND_OPENAL "OpenAL"
#define BACKEND_PULSEAUDIO "Pulse"
#define BACKEND_XAUDIO2 "XAudio2"
#define BACKEND_OPENSLES "OpenSLES"

enum GPUDeterminismMode
{
  GPU_DETERMINISM_AUTO,
  GPU_DETERMINISM_NONE,
  // This is currently the only mode.  There will probably be at least
  // one more at some point.
  GPU_DETERMINISM_FAKE_COMPLETION,
};

struct BootParameters;

struct SConfig
{
  // Wii Devices
  bool m_WiiSDCard;
  bool m_WiiKeyboard;
  bool m_WiimoteContinuousScanning;
  bool m_WiimoteEnableSpeaker;

  // ISO folder
  std::vector<std::string> m_ISOFolder;
  bool m_RecursiveISOFolder;

  // Settings
  bool bEnableDebugging = false;
#ifdef USE_GDBSTUB
  int iGDBPort;
#ifndef _WIN32
  std::string gdb_socket;
#endif
#endif
  bool bAutomaticStart = false;
  bool bBootToPause = false;

  int iCPUCore;  // Uses the values of PowerPC::CPUCore

  bool bJITNoBlockCache = false;
  bool bJITNoBlockLinking = false;
  bool bJITOff = false;
  bool bJITLoadStoreOff = false;
  bool bJITLoadStorelXzOff = false;
  bool bJITLoadStorelwzOff = false;
  bool bJITLoadStorelbzxOff = false;
  bool bJITLoadStoreFloatingOff = false;
  bool bJITLoadStorePairedOff = false;
  bool bJITFloatingPointOff = false;
  bool bJITIntegerOff = false;
  bool bJITPairedOff = false;
  bool bJITSystemRegistersOff = false;
  bool bJITBranchOff = false;

  bool bFastmem;
  bool bFPRF = false;
  bool bAccurateNaNs = false;

  int iTimingVariance = 40;  // in milli secounds
  bool bCPUThread = true;
  bool bDSPThread = false;
  bool bDSPHLE = true;
  bool bSyncGPUOnSkipIdleHack = true;
  bool bForceNTSCJ = false;
  bool bHLE_BS2 = true;
  bool bEnableCheats = false;
  bool bEnableMemcardSdWriting = true;
  bool bCopyWiiSaveNetplay = true;

  bool bDPL2Decoder = false;
  int iLatency = 20;
  bool m_audio_stretch = false;
  int m_audio_stretch_max_latency = 80;

  bool bRunCompareServer = false;
  bool bRunCompareClient = false;

  bool bMMU = false;
  bool bDCBZOFF = false;
  bool bLowDCBZHack = false;
  int iBBDumpPort = 0;
  bool bFastDiscSpeed = false;

  bool bSyncGPU = false;
  int iSyncGpuMaxDistance;
  int iSyncGpuMinDistance;
  float fSyncGpuOverclock;

  int SelectedLanguage = 0;
  bool bOverrideGCLanguage = false;

  bool bWii = false;
  bool m_is_mios = false;

  // Interface settings
  bool bConfirmStop = false;
  bool bHideCursor = false;
  bool bUsePanicHandlers = true;
  bool bOnScreenDisplayMessages = true;
  std::string theme_name;

  // Display settings
  std::string strFullscreenResolution;
  int iRenderWindowXPos = std::numeric_limits<int>::min();
  int iRenderWindowYPos = std::numeric_limits<int>::min();
  int iRenderWindowWidth = -1;
  int iRenderWindowHeight = -1;
  bool bRenderWindowAutoSize = false, bKeepWindowOnTop = false;
  bool bFullscreen = false, bRenderToMain = false;
  bool bDisableScreenSaver = false;

  int iPosX, iPosY, iWidth, iHeight;

  // Analytics settings.
  std::string m_analytics_id;
  bool m_analytics_enabled = false;
  bool m_analytics_permission_asked = false;

  // Bluetooth passthrough mode settings
  bool m_bt_passthrough_enabled = false;
  int m_bt_passthrough_pid = -1;
  int m_bt_passthrough_vid = -1;
  std::string m_bt_passthrough_link_keys;

  // USB passthrough settings
  std::set<std::pair<u16, u16>> m_usb_passthrough_devices;
  bool IsUSBDeviceWhitelisted(std::pair<u16, u16> vid_pid) const;

  bool m_enable_signature_checks = true;

  // Fifo Player related settings
  bool bLoopFifoReplay = true;

  // Custom RTC
  bool bEnableCustomRTC;
  u32 m_customRTCValue;

  DiscIO::Region m_region;

  std::string m_strVideoBackend;
  std::string m_strGPUDeterminismMode;

  // set based on the string version
  GPUDeterminismMode m_GPUDeterminismMode;

  // files
  std::string m_strBootROM;
  std::string m_strSRAM;
  std::string m_strDefaultISO;
  std::string m_strWiiSDCardPath;

  std::string m_perfDir;

  std::string m_debugger_game_id;
  // TODO: remove this as soon as the ticket view hack in IOS/ES/Views is dropped.
  bool m_disc_booted_from_game_list = false;

  const std::string& GetGameID() const { return m_game_id; }
  const std::string& GetTitleDescription() const { return m_title_description; }
  u64 GetTitleID() const { return m_title_id; }
  u16 GetRevision() const { return m_revision; }
  void ResetRunningGameMetadata();
  void SetRunningGameMetadata(const DiscIO::Volume& volume, const DiscIO::Partition& partition);
  void SetRunningGameMetadata(const IOS::ES::TMDReader& tmd);

  void LoadDefaults();
  // Replaces NTSC-K with some other region, and doesn't replace non-NTSC-K regions
  static DiscIO::Region ToGameCubeRegion(DiscIO::Region region);
  // The region argument must be valid for GameCube (i.e. must not be NTSC-K)
  static const char* GetDirectoryForRegion(DiscIO::Region region);
  std::string GetBootROMPath(const std::string& region_directory) const;
  bool SetPathsAndGameMetadata(const BootParameters& boot);
  void CheckMemcardPath(std::string& memcardPath, const std::string& gameRegion, bool isSlotA);
  DiscIO::Language GetCurrentLanguage(bool wii) const;

  IniFile LoadDefaultGameIni() const;
  IniFile LoadLocalGameIni() const;
  IniFile LoadGameIni() const;

  static IniFile LoadDefaultGameIni(const std::string& id, std::optional<u16> revision);
  static IniFile LoadLocalGameIni(const std::string& id, std::optional<u16> revision);
  static IniFile LoadGameIni(const std::string& id, std::optional<u16> revision);

  std::string m_NANDPath;
  std::string m_DumpPath;

  std::string m_strMemoryCardA;
  std::string m_strMemoryCardB;
  std::string m_strGbaCartA;
  std::string m_strGbaCartB;
  ExpansionInterface::TEXIDevices m_EXIDevice[3];
  SerialInterface::SIDevices m_SIDevice[4];
  std::string m_bba_mac;

  // interface language
  std::string m_InterfaceLanguage;
  float m_EmulationSpeed;
  bool m_OCEnable;
  float m_OCFactor;
  // other interface settings
  bool m_InterfaceToolbar;
  bool m_InterfaceStatusbar;
  bool m_InterfaceLogWindow;
  bool m_InterfaceLogConfigWindow;
  bool m_InterfaceExtendedFPSInfo;
  bool m_show_active_title = false;
  bool m_use_builtin_title_database = true;
  bool m_show_development_warning;

  bool m_ListDrives;
  bool m_ListWad;
  bool m_ListElfDol;
  bool m_ListWii;
  bool m_ListGC;
  bool m_ListPal;
  bool m_ListUsa;
  bool m_ListJap;
  bool m_ListAustralia;
  bool m_ListFrance;
  bool m_ListGermany;
  bool m_ListItaly;
  bool m_ListKorea;
  bool m_ListNetherlands;
  bool m_ListRussia;
  bool m_ListSpain;
  bool m_ListTaiwan;
  bool m_ListWorld;
  bool m_ListUnknown;
  int m_ListSort;
  int m_ListSort2;

  // Game list column toggles
  bool m_showSystemColumn;
  bool m_showBannerColumn;
  bool m_showDescriptionColumn;
  bool m_showTitleColumn;
  bool m_showMakerColumn;
  bool m_showFileNameColumn;
  bool m_showIDColumn;
  bool m_showRegionColumn;
  bool m_showSizeColumn;
  bool m_showStateColumn;

  std::string m_WirelessMac;
  bool m_PauseMovie;
  bool m_ShowLag;
  bool m_ShowFrameCount;
  bool m_ShowRTC;
  std::string m_strMovieAuthor;
  unsigned int m_FrameSkip;
  bool m_DumpFrames;
  bool m_DumpFramesSilent;
  bool m_ShowInputDisplay;

  bool m_PauseOnFocusLost;

  bool m_DisableTooltips;

  // DSP settings
  bool m_DSPEnableJIT;
  bool m_DSPCaptureLog;
  bool m_DumpAudio;
  bool m_DumpAudioSilent;
  bool m_IsMuted;
  bool m_DumpUCode;
  int m_Volume;
  std::string sBackend;

  // Input settings
  bool m_BackgroundInput;
  bool m_AdapterRumble[4];
  bool m_AdapterKonga[4];

  // Network settings
  bool m_SSLDumpRead;
  bool m_SSLDumpWrite;
  bool m_SSLVerifyCert;
  bool m_SSLDumpRootCA;
  bool m_SSLDumpPeerCert;

  SConfig(const SConfig&) = delete;
  SConfig& operator=(const SConfig&) = delete;
  SConfig(SConfig&&) = delete;
  SConfig& operator=(SConfig&&) = delete;

  // Save settings
  void SaveSettings();

  // Load settings
  void LoadSettings();

  // Return the permanent and somewhat globally used instance of this struct
  static SConfig& GetInstance() { return (*m_Instance); }
  static void Init();
  static void Shutdown();

private:
  SConfig();
  ~SConfig();

  void SaveGeneralSettings(IniFile& ini);
  void SaveInterfaceSettings(IniFile& ini);
  void SaveDisplaySettings(IniFile& ini);
  void SaveGameListSettings(IniFile& ini);
  void SaveCoreSettings(IniFile& ini);
  void SaveDSPSettings(IniFile& ini);
  void SaveInputSettings(IniFile& ini);
  void SaveMovieSettings(IniFile& ini);
  void SaveFifoPlayerSettings(IniFile& ini);
  void SaveNetworkSettings(IniFile& ini);
  void SaveAnalyticsSettings(IniFile& ini);
  void SaveBluetoothPassthroughSettings(IniFile& ini);
  void SaveUSBPassthroughSettings(IniFile& ini);

  void LoadGeneralSettings(IniFile& ini);
  void LoadInterfaceSettings(IniFile& ini);
  void LoadDisplaySettings(IniFile& ini);
  void LoadGameListSettings(IniFile& ini);
  void LoadCoreSettings(IniFile& ini);
  void LoadDSPSettings(IniFile& ini);
  void LoadInputSettings(IniFile& ini);
  void LoadMovieSettings(IniFile& ini);
  void LoadFifoPlayerSettings(IniFile& ini);
  void LoadNetworkSettings(IniFile& ini);
  void LoadAnalyticsSettings(IniFile& ini);
  void LoadBluetoothPassthroughSettings(IniFile& ini);
  void LoadUSBPassthroughSettings(IniFile& ini);

  void SetRunningGameMetadata(const std::string& game_id, u64 title_id, u16 revision,
                              Core::TitleDatabase::TitleType type);

  static SConfig* m_Instance;

  std::string m_game_id;
  std::string m_title_description;
  u64 m_title_id;
  u16 m_revision;
};
