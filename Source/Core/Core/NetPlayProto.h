// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace DiscIO
{
enum class Region;
}
namespace IOS::HLE::FS
{
class FileSystem;
}
namespace PowerPC
{
enum class CPUCore;
}

namespace NetPlay
{
struct NetSettings
{
  bool m_CPUthread = false;
  PowerPC::CPUCore m_CPUcore{};
  bool m_EnableCheats = false;
  int m_SelectedLanguage = 0;
  bool m_OverrideRegionSettings = false;
  bool m_DSPHLE = false;
  bool m_DSPEnableJIT = false;
  bool m_WriteToMemcard = false;
  bool m_RAMOverrideEnable = false;
  u32 m_Mem1Size = 0;
  u32 m_Mem2Size = 0;
  DiscIO::Region m_FallbackRegion{};
  bool m_AllowSDWrites = false;
  bool m_CopyWiiSave = false;
  bool m_OCEnable = false;
  float m_OCFactor = 0;
  std::array<ExpansionInterface::TEXIDevices, 3> m_EXIDevice{};

  std::array<u32, Config::SYSCONF_SETTINGS.size()> m_SYSCONFSettings{};

  bool m_EFBAccessEnable = false;
  bool m_BBoxEnable = false;
  bool m_ForceProgressive = false;
  bool m_EFBToTextureEnable = false;
  bool m_XFBToTextureEnable = false;
  bool m_DisableCopyToVRAM = false;
  bool m_ImmediateXFBEnable = false;
  bool m_EFBEmulateFormatChanges = false;
  int m_SafeTextureCacheColorSamples = 0;
  bool m_PerfQueriesEnable = false;
  bool m_FPRF = false;
  bool m_AccurateNaNs = false;
  bool m_DisableICache = false;
  bool m_SyncOnSkipIdle = false;
  bool m_SyncGPU = false;
  int m_SyncGpuMaxDistance = 0;
  int m_SyncGpuMinDistance = 0;
  float m_SyncGpuOverclock = 0;
  bool m_JITFollowBranch = false;
  bool m_FastDiscSpeed = false;
  bool m_MMU = false;
  bool m_Fastmem = false;
  bool m_SkipIPL = false;
  bool m_LoadIPLDump = false;
  bool m_VertexRounding = false;
  int m_InternalResolution = 0;
  bool m_EFBScaledCopy = false;
  bool m_FastDepthCalc = false;
  bool m_EnablePixelLighting = false;
  bool m_WidescreenHack = false;
  bool m_ForceFiltering = false;
  int m_MaxAnisotropy = 0;
  bool m_ForceTrueColor = false;
  bool m_DisableCopyFilter = false;
  bool m_DisableFog = false;
  bool m_ArbitraryMipmapDetection = false;
  float m_ArbitraryMipmapDetectionThreshold = 0;
  bool m_EnableGPUTextureDecoding = false;
  bool m_DeferEFBCopies = false;
  int m_EFBAccessTileSize = 0;
  bool m_EFBAccessDeferInvalidation = false;

  bool m_StrictSettingsSync = false;
  bool m_SyncSaveData = false;
  bool m_SyncCodes = false;
  std::string m_SaveDataRegion;
  bool m_SyncAllWiiSaves = false;
  std::array<int, 4> m_WiimoteExtension{};
  bool m_GolfMode = false;
  bool m_UseFMA = false;
  bool m_HideRemoteGBAs = false;

  // These aren't sent over the network directly
  bool m_IsHosting = false;
  bool m_HostInputAuthority = false;
  std::array<std::string, 4> m_GBARomPaths{};
};

struct NetTraversalConfig
{
  NetTraversalConfig() = default;
  NetTraversalConfig(bool use_traversal_, std::string traversal_host_, u16 traversal_port_)
      : use_traversal{use_traversal_}, traversal_host{std::move(traversal_host_)},
        traversal_port{traversal_port_}
  {
  }

  bool use_traversal = false;
  std::string traversal_host;
  u16 traversal_port = 0;
};

// messages
enum
{
  NP_MSG_PLAYER_JOIN = 0x10,
  NP_MSG_PLAYER_LEAVE = 0x11,

  NP_MSG_CHAT_MESSAGE = 0x30,

  NP_MSG_CHUNKED_DATA_START = 0x40,
  NP_MSG_CHUNKED_DATA_END = 0x41,
  NP_MSG_CHUNKED_DATA_PAYLOAD = 0x42,
  NP_MSG_CHUNKED_DATA_PROGRESS = 0x43,
  NP_MSG_CHUNKED_DATA_COMPLETE = 0x44,
  NP_MSG_CHUNKED_DATA_ABORT = 0x45,

  NP_MSG_PAD_DATA = 0x60,
  NP_MSG_PAD_MAPPING = 0x61,
  NP_MSG_PAD_BUFFER = 0x62,
  NP_MSG_PAD_HOST_DATA = 0x63,
  NP_MSG_GBA_CONFIG = 0x64,

  NP_MSG_WIIMOTE_DATA = 0x70,
  NP_MSG_WIIMOTE_MAPPING = 0x71,

  NP_MSG_GOLF_REQUEST = 0x90,
  NP_MSG_GOLF_SWITCH = 0x91,
  NP_MSG_GOLF_ACQUIRE = 0x92,
  NP_MSG_GOLF_RELEASE = 0x93,
  NP_MSG_GOLF_PREPARE = 0x94,

  NP_MSG_START_GAME = 0xA0,
  NP_MSG_CHANGE_GAME = 0xA1,
  NP_MSG_STOP_GAME = 0xA2,
  NP_MSG_DISABLE_GAME = 0xA3,
  NP_MSG_GAME_STATUS = 0xA4,
  NP_MSG_CLIENT_CAPABILITIES = 0xA5,
  NP_MSG_HOST_INPUT_AUTHORITY = 0xA6,
  NP_MSG_POWER_BUTTON = 0xA7,

  NP_MSG_TIMEBASE = 0xB0,
  NP_MSG_DESYNC_DETECTED = 0xB1,

  NP_MSG_COMPUTE_MD5 = 0xC0,
  NP_MSG_MD5_PROGRESS = 0xC1,
  NP_MSG_MD5_RESULT = 0xC2,
  NP_MSG_MD5_ABORT = 0xC3,
  NP_MSG_MD5_ERROR = 0xC4,

  NP_MSG_READY = 0xD0,
  NP_MSG_NOT_READY = 0xD1,

  NP_MSG_PING = 0xE0,
  NP_MSG_PONG = 0xE1,
  NP_MSG_PLAYER_PING_DATA = 0xE2,

  NP_MSG_SYNC_GC_SRAM = 0xF0,
  NP_MSG_SYNC_SAVE_DATA = 0xF1,
  NP_MSG_SYNC_CODES = 0xF2,
};

enum
{
  CON_ERR_SERVER_FULL = 1,
  CON_ERR_GAME_RUNNING = 2,
  CON_ERR_VERSION_MISMATCH = 3,
  CON_ERR_NAME_TOO_LONG = 4
};

enum
{
  SYNC_SAVE_DATA_NOTIFY = 0,
  SYNC_SAVE_DATA_SUCCESS = 1,
  SYNC_SAVE_DATA_FAILURE = 2,
  SYNC_SAVE_DATA_RAW = 3,
  SYNC_SAVE_DATA_GCI = 4,
  SYNC_SAVE_DATA_WII = 5,
  SYNC_SAVE_DATA_GBA = 6
};

enum
{
  SYNC_CODES_NOTIFY = 0,
  SYNC_CODES_NOTIFY_GECKO = 1,
  SYNC_CODES_NOTIFY_AR = 2,
  SYNC_CODES_DATA_GECKO = 3,
  SYNC_CODES_DATA_AR = 4,
  SYNC_CODES_SUCCESS = 5,
  SYNC_CODES_FAILURE = 6,
};

constexpr u32 MAX_NAME_LENGTH = 30;
constexpr size_t CHUNKED_DATA_UNIT_SIZE = 16384;

enum : u8
{
  DEFAULT_CHANNEL,
  CHUNKED_DATA_CHANNEL,
  CHANNEL_COUNT
};

struct WiimoteInput
{
  u8 report_id = 0;
  std::vector<u8> data;
};
using MessageId = u8;
using PlayerId = u8;
using FrameNum = u32;
using PadIndex = s8;
using PadMappingArray = std::array<PlayerId, 4>;
struct GBAConfig
{
  bool enabled = false;
  bool has_rom = false;
  std::string title;
  std::array<u8, 20> hash{};
};
using GBAConfigArray = std::array<GBAConfig, 4>;

struct PadDetails
{
  std::string player_name{};
  bool is_local = false;
  int local_pad = 0;
  bool hide_gba = false;
};

std::string GetPlayerMappingString(PlayerId pid, const PadMappingArray& pad_map,
                                   const GBAConfigArray& gba_config,
                                   const PadMappingArray& wiimote_map);
bool IsNetPlayRunning();
// Precondition: A netplay client instance must be present. In other words,
//               IsNetPlayRunning() must be true before calling this.
const NetSettings& GetNetSettings();
IOS::HLE::FS::FileSystem* GetWiiSyncFS();
const std::vector<u64>& GetWiiSyncTitles();
void SetWiiSyncData(std::unique_ptr<IOS::HLE::FS::FileSystem> fs, const std::vector<u64>& titles);
void ClearWiiSyncData();
void SetSIPollBatching(bool state);
void SendPowerButtonEvent();
bool IsSyncingAllWiiSaves();
void SetupWiimotes();
std::string GetGBASavePath(int pad_num);
PadDetails GetPadDetails(int pad_num);
}  // namespace NetPlay
