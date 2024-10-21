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
#include "Core/HW/Sram.h"
#include "VideoCommon/VideoConfig.h"

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
  bool cpu_thread = false;
  PowerPC::CPUCore cpu_core{};
  bool enable_cheats = false;
  int selected_language = 0;
  bool override_region_settings = false;
  bool dsp_hle = false;
  bool dsp_enable_jit = false;
  bool ram_override_enable = false;
  u32 mem1_size = 0;
  u32 mem2_size = 0;
  DiscIO::Region fallback_region{};
  bool allow_sd_writes = false;
  bool oc_enable = false;
  float oc_factor = 0;
  Common::EnumMap<ExpansionInterface::EXIDeviceType, ExpansionInterface::MAX_SLOT> exi_device{};
  int memcard_size_override = -1;

  std::array<u32, Config::SYSCONF_SETTINGS.size()> sysconf_settings{};

  bool efb_access_enable = false;
  bool bbox_enable = false;
  bool force_progressive = false;
  bool efb_to_texture_enable = false;
  bool xfb_to_texture_enable = false;
  bool disable_copy_to_vram = false;
  bool immediate_xfb_enable = false;
  bool efb_emulate_format_changes = false;
  int safe_texture_cache_color_samples = 0;
  bool perf_queries_enable = false;
  bool float_exceptions = false;
  bool divide_by_zero_exceptions = false;
  bool alignment_exceptions = false;
  bool fprf = false;
  bool accurate_nans = false;
  bool disable_icache = false;
  bool sync_on_skip_idle = false;
  bool sync_gpu = false;
  int sync_gpu_max_distance = 0;
  int sync_gpu_min_distance = 0;
  float sync_gpu_overclock = 0;
  bool jit_follow_branch = false;
  bool fast_disc_speed = false;
  bool mmu = false;
  bool fastmem = false;
  bool skip_ipl = false;
  bool load_ipl_dump = false;
  bool vertex_rounding = false;
  int internal_resolution = 0;
  bool efb_scaled_copy = false;
  bool fast_depth_calc = false;
  bool enable_pixel_lighting = false;
  bool widescreen_hack = false;
  TextureFilteringMode force_texture_filtering = TextureFilteringMode::Default;
  int max_anisotropy = 0;
  bool force_true_color = false;
  bool disable_copy_filter = false;
  bool disable_fog = false;
  bool arbitrary_mipmap_detection = false;
  float arbitrary_mipmap_detection_threshold = 0;
  bool enable_gpu_texture_decoding = false;
  bool defer_efb_copies = false;
  int efb_access_tile_size = 0;
  bool efb_access_defer_invalidation = false;

  bool savedata_load = false;
  bool savedata_write = false;
  bool savedata_sync_all_wii = false;

  bool strict_settings_sync = false;
  bool sync_codes = false;
  std::string save_data_region;
  bool golf_mode = false;
  bool use_fma = false;
  bool hide_remote_gbas = false;

  Sram sram;

  // These aren't sent over the network directly
  bool is_hosting = false;
  std::array<std::string, 4> gba_rom_paths{};
};

struct NetTraversalConfig
{
  NetTraversalConfig() = default;
  NetTraversalConfig(bool use_traversal_, std::string traversal_host_, u16 traversal_port_,
                     u16 traversal_port_alt_ = 0)
      : use_traversal{use_traversal_}, traversal_host{std::move(traversal_host_)},
        traversal_port{traversal_port_}, traversal_port_alt{traversal_port_alt_}
  {
  }

  bool use_traversal = false;
  std::string traversal_host;
  u16 traversal_port = 0;
  u16 traversal_port_alt = 0;
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

  ComputeGameDigest = 0xC0,
  GameDigestProgress = 0xC1,
  GameDigestResult = 0xC2,
  GameDigestAbort = 0xC3,
  GameDigestError = 0xC4,

  Ready = 0xD0,
  NotReady = 0xD1,

  Ping = 0xE0,
  Pong = 0xE1,
  PlayerPingData = 0xE2,

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
constexpr u32 MAX_ENET_MTU = 1392;  // see https://github.com/lsalzman/enet/issues/132

enum : u8
{
  DEFAULT_CHANNEL,
  CHUNKED_DATA_CHANNEL,
  CHANNEL_COUNT
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
void SetSIPollBatching(bool state);
void SendPowerButtonEvent();
std::string GetGBASavePath(int pad_num);
PadDetails GetPadDetails(int pad_num);
int NumLocalWiimotes();
}  // namespace NetPlay
