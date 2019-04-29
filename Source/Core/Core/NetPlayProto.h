// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>
#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_Device.h"

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
  bool m_CPUthread;
  PowerPC::CPUCore m_CPUcore;
  bool m_EnableCheats;
  int m_SelectedLanguage;
  bool m_OverrideGCLanguage;
  bool m_ProgressiveScan;
  bool m_PAL60;
  bool m_DSPHLE;
  bool m_DSPEnableJIT;
  bool m_WriteToMemcard;
  bool m_CopyWiiSave;
  bool m_ReducePollingRate;
  bool m_OCEnable;
  float m_OCFactor;
  std::array<ExpansionInterface::TEXIDevices, 3> m_EXIDevice;
  bool m_EFBAccessEnable;
  bool m_BBoxEnable;
  bool m_ForceProgressive;
  bool m_EFBToTextureEnable;
  bool m_XFBToTextureEnable;
  bool m_DisableCopyToVRAM;
  bool m_ImmediateXFBEnable;
  bool m_EFBEmulateFormatChanges;
  int m_SafeTextureCacheColorSamples;
  bool m_PerfQueriesEnable;
  bool m_FPRF;
  bool m_AccurateNaNs;
  bool m_SyncOnSkipIdle;
  bool m_SyncGPU;
  int m_SyncGpuMaxDistance;
  int m_SyncGpuMinDistance;
  float m_SyncGpuOverclock;
  bool m_JITFollowBranch;
  bool m_FastDiscSpeed;
  bool m_MMU;
  bool m_Fastmem;
  bool m_SkipIPL;
  bool m_LoadIPLDump;
  bool m_VertexRounding;
  int m_InternalResolution;
  bool m_EFBScaledCopy;
  bool m_FastDepthCalc;
  bool m_EnablePixelLighting;
  bool m_WidescreenHack;
  bool m_ForceFiltering;
  int m_MaxAnisotropy;
  bool m_ForceTrueColor;
  bool m_DisableCopyFilter;
  bool m_DisableFog;
  bool m_ArbitraryMipmapDetection;
  float m_ArbitraryMipmapDetectionThreshold;
  bool m_EnableGPUTextureDecoding;
  bool m_DeferEFBCopies;
  bool m_EFBAccessTileSize;
  bool m_EFBAccessDeferInvalidation;
  bool m_StrictSettingsSync;
  bool m_SyncSaveData;
  bool m_SyncCodes;
  std::string m_SaveDataRegion;
  bool m_SyncAllWiiSaves;
  std::array<int, 4> m_WiimoteExtension;
  bool m_GolfMode;

  // These aren't sent over the network directly
  bool m_IsHosting;
  bool m_HostInputAuthority;
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

struct Rpt : public std::vector<u8>
{
  u16 channel;
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
  NP_MSG_IPL_STATUS = 0xA5,
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
  CON_ERR_VERSION_MISMATCH = 3
};

enum
{
  SYNC_SAVE_DATA_NOTIFY = 0,
  SYNC_SAVE_DATA_SUCCESS = 1,
  SYNC_SAVE_DATA_FAILURE = 2,
  SYNC_SAVE_DATA_RAW = 3,
  SYNC_SAVE_DATA_GCI = 4,
  SYNC_SAVE_DATA_WII = 5
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

constexpr u32 NETPLAY_LZO_IN_LEN = 1024 * 64;
constexpr u32 NETPLAY_LZO_OUT_LEN = NETPLAY_LZO_IN_LEN + (NETPLAY_LZO_IN_LEN / 16) + 64 + 3;
constexpr size_t CHUNKED_DATA_UNIT_SIZE = 16384;
constexpr u8 CHANNEL_COUNT = 2;
constexpr u8 DEFAULT_CHANNEL = 0;
constexpr u8 CHUNKED_DATA_CHANNEL = 1;

using NetWiimote = std::vector<u8>;
using MessageId = u8;
using PlayerId = u8;
using FrameNum = u32;
using PadIndex = s8;
using PadMappingArray = std::array<PlayerId, 4>;

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
}  // namespace NetPlay
