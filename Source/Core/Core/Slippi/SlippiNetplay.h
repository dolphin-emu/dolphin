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
#include "Common/SPSCQueue.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "Core/Slippi/SlippiPad.h"
#include "InputCommon/GCPadStatus.h"

#ifdef _WIN32
#include <Qos2.h>
#endif

// Number of frames to wait before attempting to time-sync
#define SLIPPI_ONLINE_LOCKSTEP_INTERVAL 30
#define SLIPPI_PING_DISPLAY_INTERVAL 60
#define SLIPPI_REMOTE_PLAYER_MAX 3
#define SLIPPI_REMOTE_PLAYER_COUNT 3

struct SlippiRemotePadOutput
{
  int32_t latest_frame{};
  s32 checksum_frame{};
  u32 checksum{};
  u8 player_idx{};
  std::vector<u8> data{};
};

struct SlippiGamePrepStepResults
{
  u8 step_idx{};
  u8 char_selection{};
  u8 char_color_selection{};
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
  u32 game_idx = 0;
  u32 tiebreak_idx = 0;
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
  u8 player_idx{};
  u8 character_id{};
  u8 character_color{};
  u8 team_id{};

  bool is_character_selected = false;

  u16 stage_id{};
  bool is_stage_selected = false;
  u8 alt_stage_mode{};

  u32 rng_offset{};

  int message_id = 0;
  bool error = false;

  void Merge(SlippiPlayerSelections& s)
  {
    this->rng_offset = s.rng_offset;

    if (s.is_stage_selected)
    {
      this->stage_id = s.stage_id;
      this->is_stage_selected = true;
      this->alt_stage_mode = s.alt_stage_mode;
    }

    if (s.is_character_selected)
    {
      this->character_id = s.character_id;
      this->character_color = s.character_color;
      this->team_id = s.team_id;
      this->is_character_selected = true;
    }
  }

  void Reset()
  {
    character_id = 0;
    character_color = 0;
    is_character_selected = false;
    team_id = 0;

    stage_id = 0;
    is_stage_selected = false;

    rng_offset = 0;
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
  SlippiPlayerSelections local_player_selections;
  SlippiPlayerSelections remote_player_selections[SLIPPI_REMOTE_PLAYER_MAX];

  void Reset()
  {
    local_player_selections.Reset();
    for (int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
    {
      remote_player_selections[i].Reset();
    }
  }
};

class SlippiNetplayClient
{
public:
  void ThreadFunc();
  void SendAsync(std::unique_ptr<sf::Packet> packet);

  SlippiNetplayClient(bool is_decider);  // Make a dummy client
  SlippiNetplayClient(std::vector<std::string> addrs, std::vector<u16> ports,
                      const u8 remote_player_count, const u16 local_port, bool is_decider,
                      u8 player_idx);
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
  bool GetGamePrepResults(u8 step_idx, SlippiGamePrepStepResults& res);
  std::unique_ptr<SlippiRemotePadOutput> GetFakePadOutput(int frame);
  std::unique_ptr<SlippiRemotePadOutput> GetSlippiRemotePad(int index, int max_frame_count);
  void DropOldRemoteInputs(int32_t finalized_frame);
  SlippiMatchInfo* GetMatchInfo();
  int32_t GetSlippiLatestRemoteFrame(int max_frame_count);
  SlippiPlayerSelections GetSlippiRemoteChatMessage(bool is_chat_enabled);
  u8 GetSlippiRemoteSentChatMessage(bool is_chat_enabled);
  s32 CalcTimeOffsetUs();
  bool IsWaitingForDesyncRecovery();
  SlippiDesyncRecoveryResp GetDesyncRecoveryState();

  void WriteChatMessageToPacket(sf::Packet& packet, int message_id, u8 player_idx);
  std::unique_ptr<SlippiPlayerSelections> ReadChatMessageFromPacket(sf::Packet& packet);

  std::unique_ptr<SlippiPlayerSelections> remote_chat_message_selection =
      nullptr;  // most recent chat message player selection (message + player index)
  u8 remote_sent_chat_message_id = 0;  // most recent chat message id that current player sent

protected:
  struct
  {
    std::recursive_mutex game;
    // lock order
    std::recursive_mutex players;
    std::recursive_mutex async_queue_write;
  } m_crit;

  // SLIPPITODO: consider using AsyncQueueEntry like in NetPlayServer.h
  Common::SPSCQueue<std::unique_ptr<sf::Packet>, false> m_async_queue;

  ENetHost* m_client = nullptr;
  std::vector<ENetPeer*> m_server;
  std::thread m_thread;
  u8 m_remote_player_count = 0;

  std::string m_selected_game;
  Common::Flag m_is_running{false};
  Common::Flag m_do_loop{true};

  unsigned int m_minimum_buffer_size = 6;

  u32 m_current_game = 0;

  // Slippi Stuff
  struct FrameTiming
  {
    int32_t frame{};
    u64 time_us{};
  };

  struct FrameOffsetData
  {
    // TODO: Should the buffer size be dynamic based on time sync interval or not?
    int idx{};
    std::vector<s32> buf;
  };

  bool is_connection_selected = false;
  bool is_decider = false;
  bool has_game_started = false;
  u8 m_player_idx = 0;

  std::unordered_map<std::string, std::map<ENetPeer*, bool>> m_active_connections;

  std::deque<std::unique_ptr<SlippiPad>> m_local_pad_queue;  // most recent inputs at start of deque
  std::deque<std::unique_ptr<SlippiPad>>
      m_remote_pad_queue[SLIPPI_REMOTE_PLAYER_MAX];  // most recent inputs at start of deque
  bool is_desync_recovery = false;
  ChecksumEntry remote_checksums[SLIPPI_REMOTE_PLAYER_MAX];
  SlippiSyncedGameState remote_sync_states[SLIPPI_REMOTE_PLAYER_MAX];
  SlippiSyncedGameState local_sync_state;
  std::deque<SlippiGamePrepStepResults> game_prep_step_queue;
  u64 ping_us[SLIPPI_REMOTE_PLAYER_MAX];
  int32_t last_frame_acked[SLIPPI_REMOTE_PLAYER_MAX];
  FrameOffsetData frame_offset_data[SLIPPI_REMOTE_PLAYER_MAX];
  FrameTiming last_frame_timing[SLIPPI_REMOTE_PLAYER_MAX];
  std::array<Common::SPSCQueue<FrameTiming, false>, SLIPPI_REMOTE_PLAYER_MAX> ack_timers;

  SlippiConnectStatus slippi_connect_status = SlippiConnectStatus::NET_CONNECT_STATUS_UNSET;
  std::vector<int> failed_connections;
  SlippiMatchInfo match_info;

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
