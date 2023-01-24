// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/MainSettings.h"

#include <sstream>

#include <fmt/format.h>

#include "AudioCommon/AudioCommon.h"
#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/EnumMap.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"
#include "Core/Config/DefaultLocale.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/HSP/HSP_Device.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/PowerPC/PowerPC.h"
#include "DiscIO/Enums.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Config
{
// Main.Core

const Info<bool> MAIN_SKIP_IPL{{System::Main, "Core", "SkipIPL"}, true};
const Info<PowerPC::CPUCore> MAIN_CPU_CORE{{System::Main, "Core", "CPUCore"},
                                           PowerPC::DefaultCPUCore()};
const Info<bool> MAIN_JIT_FOLLOW_BRANCH{{System::Main, "Core", "JITFollowBranch"}, true};
const Info<bool> MAIN_FASTMEM{{System::Main, "Core", "Fastmem"}, true};
const Info<bool> MAIN_ACCURATE_CPU_CACHE{{System::Main, "Core", "AccurateCPUCache"}, false};
const Info<bool> MAIN_DSP_HLE{{System::Main, "Core", "DSPHLE"}, true};
const Info<int> MAIN_MAX_FALLBACK{{System::Main, "Core", "MaxFallback"}, 100};
const Info<int> MAIN_TIMING_VARIANCE{{System::Main, "Core", "TimingVariance"}, 40};
const Info<bool> MAIN_CPU_THREAD{{System::Main, "Core", "CPUThread"}, true};
const Info<bool> MAIN_SYNC_ON_SKIP_IDLE{{System::Main, "Core", "SyncOnSkipIdle"}, true};
const Info<std::string> MAIN_DEFAULT_ISO{{System::Main, "Core", "DefaultISO"}, ""};
const Info<bool> MAIN_ENABLE_CHEATS{{System::Main, "Core", "EnableCheats"}, false};
const Info<int> MAIN_GC_LANGUAGE{{System::Main, "Core", "SelectedLanguage"}, 0};
const Info<bool> MAIN_OVERRIDE_REGION_SETTINGS{{System::Main, "Core", "OverrideRegionSettings"},
                                               false};
const Info<bool> MAIN_DPL2_DECODER{{System::Main, "Core", "DPL2Decoder"}, false};
const Info<AudioCommon::DPL2Quality> MAIN_DPL2_QUALITY{{System::Main, "Core", "DPL2Quality"},
                                                       AudioCommon::GetDefaultDPL2Quality()};
const Info<int> MAIN_AUDIO_LATENCY{{System::Main, "Core", "AudioLatency"}, 20};
const Info<bool> MAIN_AUDIO_STRETCH{{System::Main, "Core", "AudioStretch"}, false};
const Info<int> MAIN_AUDIO_STRETCH_LATENCY{{System::Main, "Core", "AudioStretchMaxLatency"}, 80};
const Info<std::string> MAIN_MEMCARD_A_PATH{{System::Main, "Core", "MemcardAPath"}, ""};
const Info<std::string> MAIN_MEMCARD_B_PATH{{System::Main, "Core", "MemcardBPath"}, ""};
const Info<std::string>& GetInfoForMemcardPath(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));
  static constexpr Common::EnumMap<const Info<std::string>*, ExpansionInterface::MAX_MEMCARD_SLOT>
      infos{
          &MAIN_MEMCARD_A_PATH,
          &MAIN_MEMCARD_B_PATH,
      };
  return *infos[slot];
}
const Info<std::string> MAIN_AGP_CART_A_PATH{{System::Main, "Core", "AgpCartAPath"}, ""};
const Info<std::string> MAIN_AGP_CART_B_PATH{{System::Main, "Core", "AgpCartBPath"}, ""};
const Info<std::string>& GetInfoForAGPCartPath(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));
  static constexpr Common::EnumMap<const Info<std::string>*, ExpansionInterface::MAX_MEMCARD_SLOT>
      infos{
          &MAIN_AGP_CART_A_PATH,
          &MAIN_AGP_CART_B_PATH,
      };
  return *infos[slot];
}
const Info<std::string> MAIN_GCI_FOLDER_A_PATH{{System::Main, "Core", "GCIFolderAPath"}, ""};
const Info<std::string> MAIN_GCI_FOLDER_B_PATH{{System::Main, "Core", "GCIFolderBPath"}, ""};
const Info<std::string>& GetInfoForGCIPath(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));
  static constexpr Common::EnumMap<const Info<std::string>*, ExpansionInterface::MAX_MEMCARD_SLOT>
      infos{
          &MAIN_GCI_FOLDER_A_PATH,
          &MAIN_GCI_FOLDER_B_PATH,
      };
  return *infos[slot];
}
const Info<std::string> MAIN_GCI_FOLDER_A_PATH_OVERRIDE{
    {System::Main, "Core", "GCIFolderAPathOverride"}, ""};
const Info<std::string> MAIN_GCI_FOLDER_B_PATH_OVERRIDE{
    {System::Main, "Core", "GCIFolderBPathOverride"}, ""};
const Info<std::string>& GetInfoForGCIPathOverride(ExpansionInterface::Slot slot)
{
  ASSERT(ExpansionInterface::IsMemcardSlot(slot));
  static constexpr Common::EnumMap<const Info<std::string>*, ExpansionInterface::MAX_MEMCARD_SLOT>
      infos{
          &MAIN_GCI_FOLDER_A_PATH_OVERRIDE,
          &MAIN_GCI_FOLDER_B_PATH_OVERRIDE,
      };
  return *infos[slot];
}

const Info<int> MAIN_MEMORY_CARD_SIZE{{System::Main, "Core", "MemoryCardSize"}, -1};

const Info<ExpansionInterface::EXIDeviceType> MAIN_SLOT_A{
    {System::Main, "Core", "SlotA"}, ExpansionInterface::EXIDeviceType::MemoryCardFolder};
const Info<ExpansionInterface::EXIDeviceType> MAIN_SLOT_B{{System::Main, "Core", "SlotB"},
                                                          ExpansionInterface::EXIDeviceType::None};
const Info<ExpansionInterface::EXIDeviceType> MAIN_SERIAL_PORT_1{
    {System::Main, "Core", "SerialPort1"}, ExpansionInterface::EXIDeviceType::None};

const Info<ExpansionInterface::EXIDeviceType>& GetInfoForEXIDevice(ExpansionInterface::Slot slot)
{
  static constexpr Common::EnumMap<const Info<ExpansionInterface::EXIDeviceType>*,
                                   ExpansionInterface::MAX_SLOT>
      infos{
          &MAIN_SLOT_A,
          &MAIN_SLOT_B,
          &MAIN_SERIAL_PORT_1,
      };
  return *infos[slot];
}

const Info<std::string> MAIN_BBA_MAC{{System::Main, "Core", "BBA_MAC"}, ""};
const Info<std::string> MAIN_BBA_XLINK_IP{{System::Main, "Core", "BBA_XLINK_IP"}, "127.0.0.1"};
const Info<bool> MAIN_BBA_XLINK_CHAT_OSD{{System::Main, "Core", "BBA_XLINK_CHAT_OSD"}, true};

// Schthack PSO Server - https://schtserv.com/
const Info<std::string> MAIN_BBA_BUILTIN_DNS{{System::Main, "Core", "BBA_BUILTIN_DNS"},
                                             "149.56.167.128"};
const Info<std::string> MAIN_BBA_BUILTIN_IP{{System::Main, "Core", "BBA_BUILTIN_IP"}, ""};

const Info<SerialInterface::SIDevices>& GetInfoForSIDevice(int channel)
{
  static const std::array<const Info<SerialInterface::SIDevices>, 4> infos{
      Info<SerialInterface::SIDevices>{{System::Main, "Core", "SIDevice0"},
                                       SerialInterface::SIDEVICE_GC_CONTROLLER},
      Info<SerialInterface::SIDevices>{{System::Main, "Core", "SIDevice1"},
                                       SerialInterface::SIDEVICE_NONE},
      Info<SerialInterface::SIDevices>{{System::Main, "Core", "SIDevice2"},
                                       SerialInterface::SIDEVICE_NONE},
      Info<SerialInterface::SIDevices>{{System::Main, "Core", "SIDevice3"},
                                       SerialInterface::SIDEVICE_NONE},
  };
  return infos[channel];
}

const Info<bool>& GetInfoForAdapterRumble(int channel)
{
  static const std::array<const Info<bool>, 4> infos{
      Info<bool>{{System::Main, "Core", "AdapterRumble0"}, true},
      Info<bool>{{System::Main, "Core", "AdapterRumble1"}, true},
      Info<bool>{{System::Main, "Core", "AdapterRumble2"}, true},
      Info<bool>{{System::Main, "Core", "AdapterRumble3"}, true},
  };
  return infos[channel];
}

const Info<bool>& GetInfoForSimulateKonga(int channel)
{
  static const std::array<const Info<bool>, 4> infos{
      Info<bool>{{System::Main, "Core", "SimulateKonga0"}, false},
      Info<bool>{{System::Main, "Core", "SimulateKonga1"}, false},
      Info<bool>{{System::Main, "Core", "SimulateKonga2"}, false},
      Info<bool>{{System::Main, "Core", "SimulateKonga3"}, false},
  };
  return infos[channel];
}

const Info<bool> MAIN_WII_SD_CARD{{System::Main, "Core", "WiiSDCard"}, true};
const Info<bool> MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC{
    {System::Main, "Core", "WiiSDCardEnableFolderSync"}, false};
const Info<bool> MAIN_WII_KEYBOARD{{System::Main, "Core", "WiiKeyboard"}, false};
const Info<bool> MAIN_WIIMOTE_CONTINUOUS_SCANNING{
    {System::Main, "Core", "WiimoteContinuousScanning"}, false};
const Info<bool> MAIN_WIIMOTE_ENABLE_SPEAKER{{System::Main, "Core", "WiimoteEnableSpeaker"}, false};
const Info<bool> MAIN_CONNECT_WIIMOTES_FOR_CONTROLLER_INTERFACE{
    {System::Main, "Core", "WiimoteControllerInterface"}, false};
const Info<bool> MAIN_MMU{{System::Main, "Core", "MMU"}, false};
const Info<bool> MAIN_PAUSE_ON_PANIC{{System::Main, "Core", "PauseOnPanic"}, false};
const Info<int> MAIN_BB_DUMP_PORT{{System::Main, "Core", "BBDumpPort"}, -1};
const Info<bool> MAIN_SYNC_GPU{{System::Main, "Core", "SyncGPU"}, false};
const Info<int> MAIN_SYNC_GPU_MAX_DISTANCE{{System::Main, "Core", "SyncGpuMaxDistance"}, 200000};
const Info<int> MAIN_SYNC_GPU_MIN_DISTANCE{{System::Main, "Core", "SyncGpuMinDistance"}, -200000};
const Info<float> MAIN_SYNC_GPU_OVERCLOCK{{System::Main, "Core", "SyncGpuOverclock"}, 1.0f};
const Info<bool> MAIN_FAST_DISC_SPEED{{System::Main, "Core", "FastDiscSpeed"}, false};
const Info<bool> MAIN_LOW_DCBZ_HACK{{System::Main, "Core", "LowDCBZHack"}, false};
const Info<bool> MAIN_FLOAT_EXCEPTIONS{{System::Main, "Core", "FloatExceptions"}, false};
const Info<bool> MAIN_DIVIDE_BY_ZERO_EXCEPTIONS{{System::Main, "Core", "DivByZeroExceptions"},
                                                false};
const Info<bool> MAIN_FPRF{{System::Main, "Core", "FPRF"}, false};
const Info<bool> MAIN_ACCURATE_NANS{{System::Main, "Core", "AccurateNaNs"}, false};
const Info<bool> MAIN_DISABLE_ICACHE{{System::Main, "Core", "DisableICache"}, false};
const Info<float> MAIN_EMULATION_SPEED{{System::Main, "Core", "EmulationSpeed"}, 1.0f};
const Info<float> MAIN_OVERCLOCK{{System::Main, "Core", "Overclock"}, 1.0f};
const Info<bool> MAIN_OVERCLOCK_ENABLE{{System::Main, "Core", "OverclockEnable"}, false};
const Info<bool> MAIN_RAM_OVERRIDE_ENABLE{{System::Main, "Core", "RAMOverrideEnable"}, false};
const Info<u32> MAIN_MEM1_SIZE{{System::Main, "Core", "MEM1Size"}, Memory::MEM1_SIZE_RETAIL};
const Info<u32> MAIN_MEM2_SIZE{{System::Main, "Core", "MEM2Size"}, Memory::MEM2_SIZE_RETAIL};
const Info<std::string> MAIN_GFX_BACKEND{{System::Main, "Core", "GFXBackend"},
                                         VideoBackendBase::GetDefaultBackendName()};
const Info<HSP::HSPDeviceType> MAIN_HSP_DEVICE{{System::Main, "Core", "HSPDevice"},
                                               HSP::HSPDeviceType::None};
const Info<u32> MAIN_ARAM_EXPANSION_SIZE{{System::Main, "Core", "ARAMExpansionSize"}, 0x400000};

const Info<std::string> MAIN_GPU_DETERMINISM_MODE{{System::Main, "Core", "GPUDeterminismMode"},
                                                  "auto"};
const Info<s32> MAIN_OVERRIDE_BOOT_IOS{{System::Main, "Core", "OverrideBootIOS"}, -1};

GPUDeterminismMode GetGPUDeterminismMode()
{
  auto mode = Config::Get(Config::MAIN_GPU_DETERMINISM_MODE);
  if (mode == "auto")
    return GPUDeterminismMode::Auto;
  if (mode == "none")
    return GPUDeterminismMode::Disabled;
  if (mode == "fake-completion")
    return GPUDeterminismMode::FakeCompletion;

  NOTICE_LOG_FMT(CORE, "Unknown GPU determinism mode {}", mode);
  return GPUDeterminismMode::Auto;
}

const Info<std::string> MAIN_PERF_MAP_DIR{{System::Main, "Core", "PerfMapDir"}, ""};
const Info<bool> MAIN_CUSTOM_RTC_ENABLE{{System::Main, "Core", "EnableCustomRTC"}, false};
// Measured in seconds since the unix epoch (1.1.1970).  Default is 1.1.2000; there are 7 leap years
// between those dates.
const Info<u32> MAIN_CUSTOM_RTC_VALUE{{System::Main, "Core", "CustomRTCValue"},
                                      (30 * 365 + 7) * 24 * 60 * 60};
const Info<DiscIO::Region> MAIN_FALLBACK_REGION{{System::Main, "Core", "FallbackRegion"},
                                                GetDefaultRegion()};
const Info<bool> MAIN_AUTO_DISC_CHANGE{{System::Main, "Core", "AutoDiscChange"}, false};
const Info<bool> MAIN_ALLOW_SD_WRITES{{System::Main, "Core", "WiiSDCardAllowWrites"}, true};
const Info<bool> MAIN_ENABLE_SAVESTATES{{System::Main, "Core", "EnableSaveStates"}, false};
const Info<bool> MAIN_REAL_WII_REMOTE_REPEAT_REPORTS{
    {System::Main, "Core", "RealWiiRemoteRepeatReports"}, true};

// Empty means use the Dolphin default URL
const Info<std::string> MAIN_WII_NUS_SHOP_URL{{System::Main, "Core", "WiiNusShopUrl"}, ""};

// Main.Display

const Info<std::string> MAIN_FULLSCREEN_DISPLAY_RES{
    {System::Main, "Display", "FullscreenDisplayRes"}, "Auto"};
const Info<bool> MAIN_FULLSCREEN{{System::Main, "Display", "Fullscreen"}, false};
const Info<bool> MAIN_RENDER_TO_MAIN{{System::Main, "Display", "RenderToMain"}, false};
const Info<int> MAIN_RENDER_WINDOW_XPOS{{System::Main, "Display", "RenderWindowXPos"}, -1};
const Info<int> MAIN_RENDER_WINDOW_YPOS{{System::Main, "Display", "RenderWindowYPos"}, -1};
const Info<int> MAIN_RENDER_WINDOW_WIDTH{{System::Main, "Display", "RenderWindowWidth"}, 640};
const Info<int> MAIN_RENDER_WINDOW_HEIGHT{{System::Main, "Display", "RenderWindowHeight"}, 480};
const Info<bool> MAIN_RENDER_WINDOW_AUTOSIZE{{System::Main, "Display", "RenderWindowAutoSize"},
                                             false};
const Info<bool> MAIN_KEEP_WINDOW_ON_TOP{{System::Main, "Display", "KeepWindowOnTop"}, false};
const Info<bool> MAIN_DISABLE_SCREENSAVER{{System::Main, "Display", "DisableScreenSaver"}, true};

// Main.DSP

const Info<bool> MAIN_DSP_THREAD{{System::Main, "DSP", "DSPThread"}, false};
const Info<bool> MAIN_DSP_CAPTURE_LOG{{System::Main, "DSP", "CaptureLog"}, false};
const Info<bool> MAIN_DSP_JIT{{System::Main, "DSP", "EnableJIT"}, true};
const Info<bool> MAIN_DUMP_AUDIO{{System::Main, "DSP", "DumpAudio"}, false};
const Info<bool> MAIN_DUMP_AUDIO_SILENT{{System::Main, "DSP", "DumpAudioSilent"}, false};
const Info<bool> MAIN_DUMP_UCODE{{System::Main, "DSP", "DumpUCode"}, false};
const Info<std::string> MAIN_AUDIO_BACKEND{{System::Main, "DSP", "Backend"},
                                           AudioCommon::GetDefaultSoundBackend()};
const Info<int> MAIN_AUDIO_VOLUME{{System::Main, "DSP", "Volume"}, 100};
const Info<bool> MAIN_AUDIO_MUTED{{System::Main, "DSP", "Muted"}, false};
#ifdef _WIN32
const Info<std::string> MAIN_WASAPI_DEVICE{{System::Main, "DSP", "WASAPIDevice"}, "Default"};
#endif

bool ShouldUseDPL2Decoder()
{
  return Get(MAIN_DPL2_DECODER) && !Get(MAIN_DSP_HLE);
}

// Main.General

const Info<std::string> MAIN_DUMP_PATH{{System::Main, "General", "DumpPath"}, ""};
const Info<std::string> MAIN_LOAD_PATH{{System::Main, "General", "LoadPath"}, ""};
const Info<std::string> MAIN_RESOURCEPACK_PATH{{System::Main, "General", "ResourcePackPath"}, ""};
const Info<std::string> MAIN_FS_PATH{{System::Main, "General", "NANDRootPath"}, ""};
const Info<std::string> MAIN_WII_SD_CARD_IMAGE_PATH{{System::Main, "General", "WiiSDCardPath"}, ""};
const Info<std::string> MAIN_WII_SD_CARD_SYNC_FOLDER_PATH{
    {System::Main, "General", "WiiSDCardSyncFolder"}, ""};
const Info<std::string> MAIN_WFS_PATH{{System::Main, "General", "WFSPath"}, ""};
const Info<bool> MAIN_SHOW_LAG{{System::Main, "General", "ShowLag"}, false};
const Info<bool> MAIN_SHOW_FRAME_COUNT{{System::Main, "General", "ShowFrameCount"}, false};
const Info<std::string> MAIN_WIRELESS_MAC{{System::Main, "General", "WirelessMac"}, ""};
const Info<std::string> MAIN_GDB_SOCKET{{System::Main, "General", "GDBSocket"}, ""};
const Info<int> MAIN_GDB_PORT{{System::Main, "General", "GDBPort"}, -1};
const Info<int> MAIN_ISO_PATH_COUNT{{System::Main, "General", "ISOPaths"}, 0};

static Info<std::string> MakeISOPathConfigInfo(size_t idx)
{
  return Config::Info<std::string>{{Config::System::Main, "General", fmt::format("ISOPath{}", idx)},
                                   ""};
}

std::vector<std::string> GetIsoPaths()
{
  size_t count = MathUtil::SaturatingCast<size_t>(Config::Get(Config::MAIN_ISO_PATH_COUNT));
  std::vector<std::string> paths;
  paths.reserve(count);
  for (size_t i = 0; i < count; ++i)
  {
    std::string iso_path = Config::Get(MakeISOPathConfigInfo(i));
    if (!iso_path.empty())
      paths.emplace_back(std::move(iso_path));
  }
  return paths;
}

void SetIsoPaths(const std::vector<std::string>& paths)
{
  size_t old_size = MathUtil::SaturatingCast<size_t>(Config::Get(Config::MAIN_ISO_PATH_COUNT));
  size_t new_size = paths.size();

  size_t current_path_idx = 0;
  for (const std::string& p : paths)
  {
    if (p.empty())
    {
      --new_size;
      continue;
    }

    Config::SetBase(MakeISOPathConfigInfo(current_path_idx), p);
    ++current_path_idx;
  }

  for (size_t i = current_path_idx; i < old_size; ++i)
  {
    // TODO: This actually needs a Config::Erase().
    Config::SetBase(MakeISOPathConfigInfo(i), "");
  }

  Config::SetBase(Config::MAIN_ISO_PATH_COUNT, MathUtil::SaturatingCast<int>(new_size));
}

// Main.GBA

#ifdef HAS_LIBMGBA
const Info<std::string> MAIN_GBA_BIOS_PATH{{System::Main, "GBA", "BIOS"}, ""};
const std::array<Info<std::string>, 4> MAIN_GBA_ROM_PATHS{
    Info<std::string>{{System::Main, "GBA", "Rom1"}, ""},
    Info<std::string>{{System::Main, "GBA", "Rom2"}, ""},
    Info<std::string>{{System::Main, "GBA", "Rom3"}, ""},
    Info<std::string>{{System::Main, "GBA", "Rom4"}, ""}};
const Info<std::string> MAIN_GBA_SAVES_PATH{{System::Main, "GBA", "SavesPath"}, ""};
const Info<bool> MAIN_GBA_SAVES_IN_ROM_PATH{{System::Main, "GBA", "SavesInRomPath"}, false};
const Info<bool> MAIN_GBA_THREADS{{System::Main, "GBA", "Threads"}, true};
#endif

// Main.Network

const Info<bool> MAIN_NETWORK_SSL_DUMP_READ{{System::Main, "Network", "SSLDumpRead"}, false};
const Info<bool> MAIN_NETWORK_SSL_DUMP_WRITE{{System::Main, "Network", "SSLDumpWrite"}, false};
const Info<bool> MAIN_NETWORK_SSL_VERIFY_CERTIFICATES{
    {System::Main, "Network", "SSLVerifyCertificates"}, true};
const Info<bool> MAIN_NETWORK_SSL_DUMP_ROOT_CA{{System::Main, "Network", "SSLDumpRootCA"}, false};
const Info<bool> MAIN_NETWORK_SSL_DUMP_PEER_CERT{{System::Main, "Network", "SSLDumpPeerCert"},
                                                 false};
const Info<bool> MAIN_NETWORK_DUMP_BBA{{System::Main, "Network", "DumpBBA"}, false};
const Info<bool> MAIN_NETWORK_DUMP_AS_PCAP{{System::Main, "Network", "DumpAsPCAP"}, false};
// Default value based on:
//  - [RFC 1122] 4.2.3.5 TCP Connection Failures (at least 3 minutes)
//  - https://dolp.in/pr8759 hwtest (3 minutes and 10 seconds)
const Info<int> MAIN_NETWORK_TIMEOUT{{System::Main, "Network", "NetworkTimeout"}, 190};

// Main.Interface

const Info<bool> MAIN_USE_HIGH_CONTRAST_TOOLTIPS{
    {System::Main, "Interface", "UseHighContrastTooltips"}, true};
const Info<bool> MAIN_USE_PANIC_HANDLERS{{System::Main, "Interface", "UsePanicHandlers"}, true};
const Info<bool> MAIN_ABORT_ON_PANIC_ALERT{{System::Main, "Interface", "AbortOnPanicAlert"}, false};
const Info<bool> MAIN_OSD_MESSAGES{{System::Main, "Interface", "OnScreenDisplayMessages"}, true};
const Info<bool> MAIN_SKIP_NKIT_WARNING{{System::Main, "Interface", "SkipNKitWarning"}, false};
const Info<bool> MAIN_CONFIRM_ON_STOP{{System::Main, "Interface", "ConfirmStop"}, true};
const Info<ShowCursor> MAIN_SHOW_CURSOR{{System::Main, "Interface", "CursorVisibility"},
                                        ShowCursor::OnMovement};
const Info<bool> MAIN_LOCK_CURSOR{{System::Main, "Interface", "LockCursor"}, false};
const Info<std::string> MAIN_INTERFACE_LANGUAGE{{System::Main, "Interface", "LanguageCode"}, ""};
const Info<bool> MAIN_SHOW_ACTIVE_TITLE{{System::Main, "Interface", "ShowActiveTitle"}, true};
const Info<bool> MAIN_USE_BUILT_IN_TITLE_DATABASE{
    {System::Main, "Interface", "UseBuiltinTitleDatabase"}, true};
const Info<std::string> MAIN_THEME_NAME{{System::Main, "Interface", "ThemeName"},
                                        DEFAULT_THEME_DIR};
const Info<bool> MAIN_PAUSE_ON_FOCUS_LOST{{System::Main, "Interface", "PauseOnFocusLost"}, false};
const Info<bool> MAIN_ENABLE_DEBUGGING{{System::Main, "Interface", "DebugModeEnabled"}, false};

// Main.Analytics

const Info<std::string> MAIN_ANALYTICS_ID{{System::Main, "Analytics", "ID"}, ""};
const Info<bool> MAIN_ANALYTICS_ENABLED{{System::Main, "Analytics", "Enabled"}, false};
const Info<bool> MAIN_ANALYTICS_PERMISSION_ASKED{{System::Main, "Analytics", "PermissionAsked"},
                                                 false};

// Main.GameList

const Info<bool> MAIN_GAMELIST_LIST_DRIVES{{System::Main, "GameList", "ListDrives"}, false};
const Info<bool> MAIN_GAMELIST_LIST_WAD{{System::Main, "GameList", "ListWad"}, true};
const Info<bool> MAIN_GAMELIST_LIST_ELF_DOL{{System::Main, "GameList", "ListElfDol"}, true};
const Info<bool> MAIN_GAMELIST_LIST_WII{{System::Main, "GameList", "ListWii"}, true};
const Info<bool> MAIN_GAMELIST_LIST_GC{{System::Main, "GameList", "ListGC"}, true};
const Info<bool> MAIN_GAMELIST_LIST_JPN{{System::Main, "GameList", "ListJap"}, true};
const Info<bool> MAIN_GAMELIST_LIST_PAL{{System::Main, "GameList", "ListPal"}, true};
const Info<bool> MAIN_GAMELIST_LIST_USA{{System::Main, "GameList", "ListUsa"}, true};
const Info<bool> MAIN_GAMELIST_LIST_AUSTRALIA{{System::Main, "GameList", "ListAustralia"}, true};
const Info<bool> MAIN_GAMELIST_LIST_FRANCE{{System::Main, "GameList", "ListFrance"}, true};
const Info<bool> MAIN_GAMELIST_LIST_GERMANY{{System::Main, "GameList", "ListGermany"}, true};
const Info<bool> MAIN_GAMELIST_LIST_ITALY{{System::Main, "GameList", "ListItaly"}, true};
const Info<bool> MAIN_GAMELIST_LIST_KOREA{{System::Main, "GameList", "ListKorea"}, true};
const Info<bool> MAIN_GAMELIST_LIST_NETHERLANDS{{System::Main, "GameList", "ListNetherlands"},
                                                true};
const Info<bool> MAIN_GAMELIST_LIST_RUSSIA{{System::Main, "GameList", "ListRussia"}, true};
const Info<bool> MAIN_GAMELIST_LIST_SPAIN{{System::Main, "GameList", "ListSpain"}, true};
const Info<bool> MAIN_GAMELIST_LIST_TAIWAN{{System::Main, "GameList", "ListTaiwan"}, true};
const Info<bool> MAIN_GAMELIST_LIST_WORLD{{System::Main, "GameList", "ListWorld"}, true};
const Info<bool> MAIN_GAMELIST_LIST_UNKNOWN{{System::Main, "GameList", "ListUnknown"}, true};
const Info<int> MAIN_GAMELIST_LIST_SORT{{System::Main, "GameList", "ListSort"}, 3};
const Info<int> MAIN_GAMELIST_LIST_SORT_SECONDARY{{System::Main, "GameList", "ListSortSecondary"},
                                                  0};
const Info<bool> MAIN_GAMELIST_COLUMN_PLATFORM{{System::Main, "GameList", "ColumnPlatform"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_DESCRIPTION{{System::Main, "GameList", "ColumnDescription"},
                                                  false};
const Info<bool> MAIN_GAMELIST_COLUMN_BANNER{{System::Main, "GameList", "ColumnBanner"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_TITLE{{System::Main, "GameList", "ColumnTitle"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_MAKER{{System::Main, "GameList", "ColumnNotes"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_FILE_NAME{{System::Main, "GameList", "ColumnFileName"},
                                                false};
const Info<bool> MAIN_GAMELIST_COLUMN_FILE_PATH{{System::Main, "GameList", "ColumnFilePath"},
                                                false};
const Info<bool> MAIN_GAMELIST_COLUMN_GAME_ID{{System::Main, "GameList", "ColumnID"}, false};
const Info<bool> MAIN_GAMELIST_COLUMN_REGION{{System::Main, "GameList", "ColumnRegion"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_FILE_SIZE{{System::Main, "GameList", "ColumnSize"}, true};
const Info<bool> MAIN_GAMELIST_COLUMN_FILE_FORMAT{{System::Main, "GameList", "ColumnFileFormat"},
                                                  false};
const Info<bool> MAIN_GAMELIST_COLUMN_BLOCK_SIZE{{System::Main, "GameList", "ColumnBlockSize"},
                                                 false};
const Info<bool> MAIN_GAMELIST_COLUMN_COMPRESSION{{System::Main, "GameList", "ColumnCompression"},
                                                  false};
const Info<bool> MAIN_GAMELIST_COLUMN_TAGS{{System::Main, "GameList", "ColumnTags"}, false};

// Main.FifoPlayer

const Info<bool> MAIN_FIFOPLAYER_LOOP_REPLAY{{System::Main, "FifoPlayer", "LoopReplay"}, true};
const Info<bool> MAIN_FIFOPLAYER_EARLY_MEMORY_UPDATES{
    {System::Main, "FifoPlayer", "EarlyMemoryUpdates"}, false};

// Main.AutoUpdate

const Info<std::string> MAIN_AUTOUPDATE_UPDATE_TRACK{{System::Main, "AutoUpdate", "UpdateTrack"},
                                                     Common::GetScmUpdateTrackStr()};
const Info<std::string> MAIN_AUTOUPDATE_HASH_OVERRIDE{{System::Main, "AutoUpdate", "HashOverride"},
                                                      ""};

// Main.Movie

const Info<bool> MAIN_MOVIE_PAUSE_MOVIE{{System::Main, "Movie", "PauseMovie"}, false};
const Info<std::string> MAIN_MOVIE_MOVIE_AUTHOR{{System::Main, "Movie", "Author"}, ""};
const Info<bool> MAIN_MOVIE_DUMP_FRAMES{{System::Main, "Movie", "DumpFrames"}, false};
const Info<bool> MAIN_MOVIE_DUMP_FRAMES_SILENT{{System::Main, "Movie", "DumpFramesSilent"}, false};
const Info<bool> MAIN_MOVIE_SHOW_INPUT_DISPLAY{{System::Main, "Movie", "ShowInputDisplay"}, false};
const Info<bool> MAIN_MOVIE_SHOW_RTC{{System::Main, "Movie", "ShowRTC"}, false};
const Info<bool> MAIN_MOVIE_SHOW_RERECORD{{System::Main, "Movie", "ShowRerecord"}, false};

// Main.Input

const Info<bool> MAIN_INPUT_BACKGROUND_INPUT{{System::Main, "Input", "BackgroundInput"}, false};

// Main.Debug

const Info<bool> MAIN_DEBUG_JIT_OFF{{System::Main, "Debug", "JitOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_OFF{{System::Main, "Debug", "JitLoadStoreOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LXZ_OFF{{System::Main, "Debug", "JitLoadStorelXzOff"},
                                                   false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LWZ_OFF{{System::Main, "Debug", "JitLoadStorelwzOff"},
                                                   false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_LBZX_OFF{{System::Main, "Debug", "JitLoadStorelbzxOff"},
                                                    false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_FLOATING_OFF{
    {System::Main, "Debug", "JitLoadStoreFloatingOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_LOAD_STORE_PAIRED_OFF{
    {System::Main, "Debug", "JitLoadStorePairedOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_FLOATING_POINT_OFF{{System::Main, "Debug", "JitFloatingPointOff"},
                                                   false};
const Info<bool> MAIN_DEBUG_JIT_INTEGER_OFF{{System::Main, "Debug", "JitIntegerOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_PAIRED_OFF{{System::Main, "Debug", "JitPairedOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_SYSTEM_REGISTERS_OFF{
    {System::Main, "Debug", "JitSystemRegistersOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_BRANCH_OFF{{System::Main, "Debug", "JitBranchOff"}, false};
const Info<bool> MAIN_DEBUG_JIT_REGISTER_CACHE_OFF{{System::Main, "Debug", "JitRegisterCacheOff"},
                                                   false};

// Main.BluetoothPassthrough

const Info<bool> MAIN_BLUETOOTH_PASSTHROUGH_ENABLED{
    {System::Main, "BluetoothPassthrough", "Enabled"}, false};
const Info<int> MAIN_BLUETOOTH_PASSTHROUGH_VID{{System::Main, "BluetoothPassthrough", "VID"}, -1};
const Info<int> MAIN_BLUETOOTH_PASSTHROUGH_PID{{System::Main, "BluetoothPassthrough", "PID"}, -1};
const Info<std::string> MAIN_BLUETOOTH_PASSTHROUGH_LINK_KEYS{
    {System::Main, "BluetoothPassthrough", "LinkKeys"}, ""};

// Main.USBPassthrough

const Info<std::string> MAIN_USB_PASSTHROUGH_DEVICES{{System::Main, "USBPassthrough", "Devices"},
                                                     ""};

static std::set<std::pair<u16, u16>> LoadUSBWhitelistFromString(const std::string& devices_string)
{
  std::set<std::pair<u16, u16>> devices;
  for (const auto& pair : SplitString(devices_string, ','))
  {
    const auto index = pair.find(':');
    if (index == std::string::npos)
      continue;

    const u16 vid = static_cast<u16>(strtol(pair.substr(0, index).c_str(), nullptr, 16));
    const u16 pid = static_cast<u16>(strtol(pair.substr(index + 1).c_str(), nullptr, 16));
    if (vid && pid)
      devices.emplace(vid, pid);
  }
  return devices;
}

static std::string SaveUSBWhitelistToString(const std::set<std::pair<u16, u16>>& devices)
{
  std::ostringstream oss;
  for (const auto& device : devices)
    oss << fmt::format("{:04x}:{:04x}", device.first, device.second) << ',';
  std::string devices_string = oss.str();
  if (!devices_string.empty())
    devices_string.pop_back();
  return devices_string;
}

std::set<std::pair<u16, u16>> GetUSBDeviceWhitelist()
{
  return LoadUSBWhitelistFromString(Config::Get(Config::MAIN_USB_PASSTHROUGH_DEVICES));
}

void SetUSBDeviceWhitelist(const std::set<std::pair<u16, u16>>& devices)
{
  Config::SetBase(Config::MAIN_USB_PASSTHROUGH_DEVICES, SaveUSBWhitelistToString(devices));
}

// Main.EmulatedUSBDevices

const Info<bool> MAIN_EMULATE_SKYLANDER_PORTAL{
    {System::Main, "EmulatedUSBDevices", "EmulateSkylanderPortal"}, false};

const Info<bool> MAIN_EMULATE_INFINITY_BASE{
    {System::Main, "EmulatedUSBDevices", "EmulateInfinityBase"}, false};

// The reason we need this function is because some memory card code
// expects to get a non-NTSC-K region even if we're emulating an NTSC-K Wii.
DiscIO::Region ToGameCubeRegion(DiscIO::Region region)
{
  if (region != DiscIO::Region::NTSC_K)
    return region;

  // GameCube has no NTSC-K region. No choice of replacement value is completely
  // non-arbitrary, but let's go with NTSC-J since Korean GameCubes are NTSC-J.
  return DiscIO::Region::NTSC_J;
}

const char* GetDirectoryForRegion(DiscIO::Region region, RegionDirectoryStyle style)
{
  if (region == DiscIO::Region::Unknown)
    region = ToGameCubeRegion(Config::Get(Config::MAIN_FALLBACK_REGION));

  switch (region)
  {
  case DiscIO::Region::NTSC_J:
    return style == RegionDirectoryStyle::Legacy ? JAP_DIR : JPN_DIR;

  case DiscIO::Region::NTSC_U:
    return USA_DIR;

  case DiscIO::Region::PAL:
    return EUR_DIR;

  case DiscIO::Region::NTSC_K:
    // See ToGameCubeRegion
    ASSERT_MSG(BOOT, false, "NTSC-K is not a valid GameCube region");
    return style == RegionDirectoryStyle::Legacy ? JAP_DIR : JPN_DIR;

  default:
    ASSERT_MSG(BOOT, false, "Default case should not be reached");
    return EUR_DIR;
  }
}

std::string GetBootROMPath(const std::string& region_directory)
{
  const std::string path =
      File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + region_directory + DIR_SEP GC_IPL;
  if (!File::Exists(path))
    return File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + region_directory + DIR_SEP GC_IPL;
  return path;
}

std::string GetMemcardPath(ExpansionInterface::Slot slot, std::optional<DiscIO::Region> region,
                           u16 size_mb)
{
  return GetMemcardPath(Config::Get(GetInfoForMemcardPath(slot)), slot, region, size_mb);
}

std::string GetMemcardPath(std::string configured_filename, ExpansionInterface::Slot slot,
                           std::optional<DiscIO::Region> region, u16 size_mb)
{
  const std::string blocks_string = size_mb < Memcard::MBIT_SIZE_MEMORY_CARD_2043 ?
                                        fmt::format(".{}", Memcard::MbitToFreeBlocks(size_mb)) :
                                        "";

  if (configured_filename.empty())
  {
    // Use default memcard path if there is no user defined one.
    const bool is_slot_a = slot == ExpansionInterface::Slot::A;
    const std::string region_string = Config::GetDirectoryForRegion(
        Config::ToGameCubeRegion(region ? *region : Config::Get(Config::MAIN_FALLBACK_REGION)));
    return fmt::format("{}{}.{}{}.raw", File::GetUserPath(D_GCUSER_IDX),
                       is_slot_a ? GC_MEMCARDA : GC_MEMCARDB, region_string, blocks_string);
  }

  // Custom path is expected to be stored in the form of
  // "/path/to/file.{region_code}.raw"
  // with an arbitrary but supported region code.
  // Try to extract and replace that region code.
  // If there's no region code just insert one before the extension.

  std::string dir;
  std::string name;
  std::string ext;
  UnifyPathSeparators(configured_filename);
  SplitPath(configured_filename, &dir, &name, &ext);

  constexpr std::string_view us_region = "." USA_DIR;
  constexpr std::string_view jp_region = "." JAP_DIR;
  constexpr std::string_view eu_region = "." EUR_DIR;
  std::optional<DiscIO::Region> path_region = std::nullopt;
  if (name.ends_with(us_region))
  {
    name = name.substr(0, name.size() - us_region.size());
    path_region = DiscIO::Region::NTSC_U;
  }
  else if (name.ends_with(jp_region))
  {
    name = name.substr(0, name.size() - jp_region.size());
    path_region = DiscIO::Region::NTSC_J;
  }
  else if (name.ends_with(eu_region))
  {
    name = name.substr(0, name.size() - eu_region.size());
    path_region = DiscIO::Region::PAL;
  }

  const DiscIO::Region used_region =
      region ? *region : (path_region ? *path_region : Config::Get(Config::MAIN_FALLBACK_REGION));
  return fmt::format("{}{}.{}{}{}", dir, name,
                     Config::GetDirectoryForRegion(Config::ToGameCubeRegion(used_region)),
                     blocks_string, ext);
}

bool IsDefaultMemcardPathConfigured(ExpansionInterface::Slot slot)
{
  return Config::Get(GetInfoForMemcardPath(slot)).empty();
}

std::string GetGCIFolderPath(ExpansionInterface::Slot slot, std::optional<DiscIO::Region> region)
{
  return GetGCIFolderPath(Config::Get(GetInfoForGCIPath(slot)), slot, region);
}

std::string GetGCIFolderPath(std::string configured_folder, ExpansionInterface::Slot slot,
                             std::optional<DiscIO::Region> region)
{
  if (configured_folder.empty())
  {
    const auto region_dir = Config::GetDirectoryForRegion(
        Config::ToGameCubeRegion(region ? *region : Config::Get(Config::MAIN_FALLBACK_REGION)));
    const bool is_slot_a = slot == ExpansionInterface::Slot::A;
    return fmt::format("{}{}/Card {}", File::GetUserPath(D_GCUSER_IDX), region_dir,
                       is_slot_a ? 'A' : 'B');
  }

  // Custom path is expected to be stored in the form of
  // "/path/to/folder/{region_code}"
  // with an arbitrary but supported region code.
  // Try to extract and replace that region code.
  // If there's no region code just insert one at the end.

  UnifyPathSeparators(configured_folder);
  while (configured_folder.ends_with('/'))
    configured_folder.pop_back();

  constexpr std::string_view us_region = "/" USA_DIR;
  constexpr std::string_view jp_region = "/" JPN_DIR;
  constexpr std::string_view eu_region = "/" EUR_DIR;
  std::string_view base_path = configured_folder;
  std::optional<DiscIO::Region> path_region = std::nullopt;
  if (base_path.ends_with(us_region))
  {
    base_path = base_path.substr(0, base_path.size() - us_region.size());
    path_region = DiscIO::Region::NTSC_U;
  }
  else if (base_path.ends_with(jp_region))
  {
    base_path = base_path.substr(0, base_path.size() - jp_region.size());
    path_region = DiscIO::Region::NTSC_J;
  }
  else if (base_path.ends_with(eu_region))
  {
    base_path = base_path.substr(0, base_path.size() - eu_region.size());
    path_region = DiscIO::Region::PAL;
  }

  const DiscIO::Region used_region =
      region ? *region : (path_region ? *path_region : Config::Get(Config::MAIN_FALLBACK_REGION));
  return fmt::format("{}/{}", base_path,
                     Config::GetDirectoryForRegion(Config::ToGameCubeRegion(used_region),
                                                   Config::RegionDirectoryStyle::Modern));
}

bool IsDefaultGCIFolderPathConfigured(ExpansionInterface::Slot slot)
{
  return Config::Get(GetInfoForGCIPath(slot)).empty();
}
}  // namespace Config
