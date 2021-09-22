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
  bool m_CPUthread;
  PowerPC::CPUCore m_CPUcore;
  bool m_EnableCheats;
  int m_SelectedLanguage;
  bool m_OverrideRegionSettings;
  bool m_DSPHLE;
  bool m_DSPEnableJIT;
  bool m_WriteToMemcard;
  bool m_RAMOverrideEnable;
  u32 m_Mem1Size;
  u32 m_Mem2Size;
  DiscIO::Region m_FallbackRegion;
  bool m_AllowSDWrites;
  bool m_CopyWiiSave;
  bool m_OCEnable;
  float m_OCFactor;
  std::array<ExpansionInterface::TEXIDevices, 3> m_EXIDevice;

  std::array<u32, Config::SYSCONF_SETTINGS.size()> m_SYSCONFSettings;

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
  bool m_DisableICache;
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
  int m_EFBAccessTileSize;
  bool m_EFBAccessDeferInvalidation;

  bool m_StrictSettingsSync;
  bool m_SyncSaveData;
  bool m_SyncCodes;
  std::string m_SaveDataRegion;
  bool m_SyncAllWiiSaves;
  std::array<int, 4> m_WiimoteExtension;
  bool m_GolfMode;
  bool m_UseFMA;
  bool m_HideRemoteGBAs;

  // These aren't sent over the network directly
  bool m_IsHosting;
  bool m_HostInputAuthority;
  std::array<std::string, 4> m_GBARomPaths;
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

enum class MessageID : u8
{
  PlayerJoin = 0x10,
  PlayerLeave = 0x11,

  ChatMessage = 0x30,

  ChunkedDataStart = 0x40,
  ChunkedDataEnd = 0x41,
  ChunkedDataPayload = 0x42,
  ChunkedDataProgress = 0x43,
  ChunkedDataComplete = 0x44,
  ChunkedDataAbort = 0x45,

  PadData = 0x60,
  PadMapping = 0x61,
  PadBuffer = 0x62,
  PadHostData = 0x63,
  GBAConfig = 0x64,

  WiimoteData = 0x70,
  WiimoteMapping = 0x71,

  GolfRequest = 0x90,
  GolfSwitch = 0x91,
  GolfAcquire = 0x92,
  GolfRelease = 0x93,
  GolfPrepare = 0x94,

  StartGame = 0xA0,
  ChangeGame = 0xA1,
  StopGame = 0xA2,
  DisableGame = 0xA3,
  GameStatus = 0xA4,
  ClientCapabilities = 0xA5,
  HostInputAuthority = 0xA6,
  PowerButton = 0xA7,

  TimeBase = 0xB0,
  DesyncDetected = 0xB1,

  ComputeMD5 = 0xC0,
  MD5Progress = 0xC1,
  MD5Result = 0xC2,
  MD5Abort = 0xC3,
  MD5Error = 0xC4,

  Ready = 0xD0,
  NotReady = 0xD1,

  Ping = 0xE0,
  Pong = 0xE1,
  PlayerPingData = 0xE2,

  SyncGCSRAM = 0xF0,
  SyncSaveData = 0xF1,
  SyncCodes = 0xF2,
};

enum
{
  CON_ERR_SERVER_FULL = 1,
  CON_ERR_GAME_RUNNING = 2,
  CON_ERR_VERSION_MISMATCH = 3,
  CON_ERR_NAME_TOO_LONG = 4
};

enum class SyncSaveDataID : u8
{
  Notify = 0,
  Success = 1,
  Failure = 2,
  RawData = 3,
  GCIData = 4,
  WiiData = 5,
  GBAData = 6
};

enum class SyncCodeID : u8
{
  Notify = 0,
  NotifyGecko = 1,
  NotifyAR = 2,
  GeckoData = 3,
  ARData = 4,
  Success = 5,
  Failure = 6,
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
  u8 report_id;
  std::vector<u8> data;
};
using MessageId = u8;
using PlayerId = u8;
using FrameNum = u32;
using PadIndex = s8;
using PadMappingArray = std::array<PlayerId, 4>;
struct GBAConfig
{
  bool enabled;
  bool has_rom;
  std::string title;
  std::array<u8, 20> hash;
};
using GBAConfigArray = std::array<GBAConfig, 4>;

struct PadDetails
{
  std::string player_name;
  bool is_local;
  int local_pad;
  bool hide_gba;
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
