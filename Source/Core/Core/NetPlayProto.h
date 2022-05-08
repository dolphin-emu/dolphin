// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/HW/EXI/EXI.h"
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
  Common::EnumMap<ExpansionInterface::EXIDeviceType, ExpansionInterface::MAX_SLOT> m_EXIDevice{};
  int m_MemcardSizeOverride = -1;

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
  bool m_FloatExceptions = false;
  bool m_DivideByZeroExceptions = false;
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

enum class MessageID : u8
{
  ConnectionSuccessful = 0,

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

enum class ConnectionError : u8
{
  NoError = 0,
  ServerFull = 1,
  GameRunning = 2,
  VersionMismatch = 3,
  NameTooLong = 4
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
  u8 report_id = 0;
  std::vector<u8> data;
};
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
void SetSIPollBatching(bool state);
void SendPowerButtonEvent();
bool IsSyncingAllWiiSaves();
void SetupWiimotes();
std::string GetGBASavePath(int pad_num);
PadDetails GetPadDetails(int pad_num);
}  // namespace NetPlay
