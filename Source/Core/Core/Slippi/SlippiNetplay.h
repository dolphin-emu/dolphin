// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "Core/Slippi/SlippiPad.h"
#include "InputCommon/GCPadStatus.h"
#ifdef _WIN32
#include <Qos2.h>
#endif

#define SLIPPI_ONLINE_LOCKSTEP_INTERVAL                                                            \
  30  // Number of frames to wait before attempting to time-sync
#define SLIPPI_PING_DISPLAY_INTERVAL 60
#define SLIPPI_REMOTE_PLAYER_MAX 3
#define SLIPPI_REMOTE_PLAYER_COUNT 3

struct SlippiRemotePadOutput
{
  int32_t latest_frame{};
  s32 checksum_frame;
  u32 checksum;
  u8 playerIdx{};
  std::vector<u8> data;
};

struct SlippiGamePrepStepResults
{
  u8 step_idx;
  u8 char_selection;
  u8 char_color_selection;
  u8 stage_selections[2];
};

struct SlippiSyncedFighterState
{
  u8 stocks_remaining = 4;
  u16 current_health = 0;
};

struct SlippiSyncedGameState
{
  std::string match_id = "";
  u32 game_index = 0;
  u32 tiebreak_index = 0;
  u32 seconds_remaining = 480;
  SlippiSyncedFighterState fighters[4];
};

struct SlippiDesyncRecoveryResp
{
  bool is_recovering = false;
  bool is_waiting = false;
  bool is_error = false;
  SlippiSyncedGameState state;
};

class SlippiPlayerSelections
{
public:
  u8 playerIdx{};
  u8 characterId{};
  u8 characterColor{};
  u8 teamId{};

  bool isCharacterSelected = false;

  u16 stageId{};
  bool isStageSelected = false;

  u32 rngOffset{};

  int messageId = 0;
  bool error = false;

  void Merge(SlippiPlayerSelections& s)
  {
    this->rngOffset = s.rngOffset;

    if (s.isStageSelected)
    {
      this->stageId = s.stageId;
      this->isStageSelected = true;
    }

    if (s.isCharacterSelected)
    {
      this->characterId = s.characterId;
      this->characterColor = s.characterColor;
      this->teamId = s.teamId;
      this->isCharacterSelected = true;
    }
  }

  void Reset()
  {
    characterId = 0;
    characterColor = 0;
    isCharacterSelected = false;
    teamId = 0;

    stageId = 0;
    isStageSelected = false;

    rngOffset = 0;
  }
};

struct ChecksumEntry
{
  s32 frame;
  u32 value;
};

class SlippiMatchInfo
{
public:
  SlippiPlayerSelections localPlayerSelections;
  SlippiPlayerSelections remotePlayerSelections[SLIPPI_REMOTE_PLAYER_MAX];

  void Reset()
  {
    localPlayerSelections.Reset();
    for (int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
    {
      remotePlayerSelections[i].Reset();
    }
  }
};

class SlippiNetplayClient
{
public:
  void ThreadFunc();
  void SendAsync(std::unique_ptr<sf::Packet> packet);

  SlippiNetplayClient(bool isDecider);  // Make a dummy client
  SlippiNetplayClient(std::vector<std::string> addrs, std::vector<u16> ports,
                      const u8 remotePlayerCount, const u16 localPort, bool isDecider,
                      u8 playerIdx);
  ~SlippiNetplayClient();

  // Slippi Online
  enum class SlippiConnectStatus
  {
    NET_CONNECT_STATUS_UNSET,
    NET_CONNECT_STATUS_INITIATED,
    NET_CONNECT_STATUS_CONNECTED,
    NET_CONNECT_STATUS_FAILED,
    NET_CONNECT_STATUS_DISCONNECTED,
  };

  bool IsDecider();
  bool IsConnectionSelected();
  u8 LocalPlayerPort();
  SlippiConnectStatus GetSlippiConnectStatus();
  std::vector<int> GetFailedConnections();
  void StartSlippiGame();
  void SendConnectionSelected();
  void SendSlippiPad(std::unique_ptr<SlippiPad> pad);
  void SetMatchSelections(SlippiPlayerSelections& s);
  void SendGamePrepStep(SlippiGamePrepStepResults& s);
  void SendSyncedGameState(SlippiSyncedGameState& s);
  bool GetGamePrepResults(u8 stepIdx, SlippiGamePrepStepResults& res);
  std::unique_ptr<SlippiRemotePadOutput> GetFakePadOutput(int frame);
  std::unique_ptr<SlippiRemotePadOutput> GetSlippiRemotePad(int index, int maxFrameCount);
  void DropOldRemoteInputs(int32_t finalizedFrame);
  SlippiMatchInfo* GetMatchInfo();
  int32_t GetSlippiLatestRemoteFrame(int maxFrameCount);
  SlippiPlayerSelections GetSlippiRemoteChatMessage(bool isChatEnabled);
  u8 GetSlippiRemoteSentChatMessage(bool isChatEnabled);
  s32 CalcTimeOffsetUs();
  bool IsWaitingForDesyncRecovery();
  SlippiDesyncRecoveryResp GetDesyncRecoveryState();

  void WriteChatMessageToPacket(sf::Packet& packet, int messageId, u8 playerIdx);
  std::unique_ptr<SlippiPlayerSelections> ReadChatMessageFromPacket(sf::Packet& packet);

  std::unique_ptr<SlippiPlayerSelections> remoteChatMessageSelection =
      nullptr;  // most recent chat message player selection (message + player index)
  u8 remoteSentChatMessageId = 0;  // most recent chat message id that current player sent

protected:
  struct
  {
    std::recursive_mutex game;
    // lock order
    std::recursive_mutex players;
    std::recursive_mutex async_queue_write;
  } m_crit;

  std::queue<std::unique_ptr<sf::Packet>> m_async_queue;

  ENetHost* m_client = nullptr;
  std::vector<ENetPeer*> m_server;
  std::thread m_thread;
  u8 m_remotePlayerCount = 0;

  std::string m_selected_game;
  Common::Flag m_is_running{false};
  Common::Flag m_do_loop{true};

  unsigned int m_minimum_buffer_size = 6;

  u32 m_current_game = 0;

  // Slippi Stuff
  struct FrameTiming
  {
    int32_t frame;
    u64 timeUs;
  };

  struct FrameOffsetData
  {
    // TODO: Should the buffer size be dynamic based on time sync interval or not?
    int idx{};
    std::vector<s32> buf;
  };

  bool isConnectionSelected = false;
  bool isDecider = false;
  bool hasGameStarted = false;
  u8 m_player_idx = 0;

  std::unordered_map<std::string, std::map<ENetPeer*, bool>> activeConnections;

  std::deque<std::unique_ptr<SlippiPad>> localPadQueue;  // most recent inputs at start of deque
  std::deque<std::unique_ptr<SlippiPad>>
      remotePadQueue[SLIPPI_REMOTE_PLAYER_MAX];  // most recent inputs at start of deque
  bool is_desync_recovery = false;
  ChecksumEntry remote_checksums[SLIPPI_REMOTE_PLAYER_MAX];
  SlippiSyncedGameState remote_sync_states[SLIPPI_REMOTE_PLAYER_MAX];
  SlippiSyncedGameState local_sync_state;
  std::deque<SlippiGamePrepStepResults> game_prep_step_queue;
  u64 pingUs[SLIPPI_REMOTE_PLAYER_MAX];
  int32_t lastFrameAcked[SLIPPI_REMOTE_PLAYER_MAX];
  FrameOffsetData frameOffsetData[SLIPPI_REMOTE_PLAYER_MAX];
  FrameTiming lastFrameTiming[SLIPPI_REMOTE_PLAYER_MAX];
  std::array<std::queue<FrameTiming>, SLIPPI_REMOTE_PLAYER_MAX> ackTimers;

  SlippiConnectStatus slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_UNSET;
  std::vector<int> failedConnections;
  SlippiMatchInfo matchInfo;

  bool m_is_recording = false;

  void writeToPacket(sf::Packet& packet, SlippiPlayerSelections& s);
  std::unique_ptr<SlippiPlayerSelections> readSelectionsFromPacket(sf::Packet& packet);

private:
  u8 PlayerIdxFromPort(u8 port);
  unsigned int OnData(sf::Packet& packet, ENetPeer* peer);
  void Send(sf::Packet& packet);
  void Disconnect();

  bool m_is_connected = false;

#ifdef _WIN32
  HANDLE m_qos_handle;
  QOS_FLOWID m_qos_flow_id;
#endif

  u32 m_timebase_frame = 0;
};

extern SlippiNetplayClient* SLIPPI_NETPLAY;  // singleton static pointer

inline bool IsOnline()
{
  return SLIPPI_NETPLAY != nullptr;
}
