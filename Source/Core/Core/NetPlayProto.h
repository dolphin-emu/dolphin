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
  ExpansionInterface::TEXIDevices m_EXIDevice[2];
  bool m_SyncSaveData;
  std::string m_SaveDataRegion;
  bool m_IsHosting;
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

extern u64 g_netplay_initial_rtc;

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

  NP_MSG_PAD_DATA = 0x60,
  NP_MSG_PAD_MAPPING = 0x61,
  NP_MSG_PAD_BUFFER = 0x62,

  NP_MSG_WIIMOTE_DATA = 0x70,
  NP_MSG_WIIMOTE_MAPPING = 0x71,

  NP_MSG_START_GAME = 0xA0,
  NP_MSG_CHANGE_GAME = 0xA1,
  NP_MSG_STOP_GAME = 0xA2,
  NP_MSG_DISABLE_GAME = 0xA3,
  NP_MSG_GAME_STATUS = 0xA4,

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

constexpr u32 NETPLAY_LZO_IN_LEN = 1024 * 64;
constexpr u32 NETPLAY_LZO_OUT_LEN = NETPLAY_LZO_IN_LEN + (NETPLAY_LZO_IN_LEN / 16) + 64 + 3;

using NetWiimote = std::vector<u8>;
using MessageId = u8;
using PlayerId = u8;
using FrameNum = u32;
using PadMapping = s8;
using PadMappingArray = std::array<PadMapping, 4>;

bool IsNetPlayRunning();
// Precondition: A netplay client instance must be present. In other words,
//               IsNetPlayRunning() must be true before calling this.
const NetSettings& GetNetSettings();
IOS::HLE::FS::FileSystem* GetWiiSyncFS();
void SetWiiSyncFS(std::unique_ptr<IOS::HLE::FS::FileSystem> fs);
void ClearWiiSyncFS();
}  // namespace NetPlay
