// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <SFML/Network/Packet.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Common/Event.h"
#include "Common/QoSSession.h"
#include "Common/SPSCQueue.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "Core/SyncIdentifier.h"
#include "InputCommon/GCPadStatus.h"
#include "UICommon/NetPlayIndex.h"

namespace NetPlay
{
class NetPlayUI;
struct SaveSyncInfo;

class NetPlayServer : public Common::TraversalClientClient
{
public:
  void ThreadFunc();
  void SendAsync(sf::Packet&& packet, PlayerId pid, u8 channel_id = DEFAULT_CHANNEL);
  void SendAsyncToClients(sf::Packet&& packet, PlayerId skip_pid = 0,
                          u8 channel_id = DEFAULT_CHANNEL);
  void SendChunked(sf::Packet&& packet, PlayerId pid, const std::string& title = "");
  void SendChunkedToClients(sf::Packet&& packet, PlayerId skip_pid = 0,
                            const std::string& title = "");

  NetPlayServer(u16 port, bool forward_port, NetPlayUI* dialog,
                const NetTraversalConfig& traversal_config);
  ~NetPlayServer();

  bool ChangeGame(const SyncIdentifier& sync_identifier, const std::string& netplay_name);
  bool ComputeGameDigest(const SyncIdentifier& sync_identifier);
  bool AbortGameDigest();
  void SendChatMessage(const std::string& msg);

  bool DoAllPlayersHaveIPLDump() const;
  bool DoAllPlayersHaveHardwareFMA() const;
  bool StartGame();
  bool RequestStartGame();
  void AbortGameStart();

  PadMappingArray GetPadMapping() const;
  void SetPadMapping(const PadMappingArray& mappings);

  GBAConfigArray GetGBAConfig() const;
  void SetGBAConfig(const GBAConfigArray& configs, bool update_rom);

  PadMappingArray GetWiimoteMapping() const;
  void SetWiimoteMapping(const PadMappingArray& mappings);

  void AdjustPadBufferSize(unsigned int size);
  void SetHostInputAuthority(bool enable);

  void KickPlayer(PlayerId player);

  u16 GetPort() const;

  std::unordered_set<std::string> GetInterfaceSet() const;
  std::string GetInterfaceHost(const std::string& inter) const;

  bool is_connected = false;

private:
  class Client
  {
  public:
    PlayerId pid{};
    std::string name;
    std::string revision;
    SyncIdentifierComparison game_status = SyncIdentifierComparison::Unknown;
    bool has_ipl_dump = false;
    bool has_hardware_fma = false;

    ENetPeer* socket = nullptr;
    u32 ping = 0;
    u32 current_game = 0;

    Common::QoSSession qos_session;

    bool operator==(const Client& other) const { return this == &other; }
    bool IsHost() const { return pid == 1; }
  };

  enum class TargetMode
  {
    Only,
    AllExcept
  };

  struct AsyncQueueEntry
  {
    sf::Packet packet;
    PlayerId target_pid{};
    TargetMode target_mode{};
    u8 channel_id = 0;
  };

  struct ChunkedDataQueueEntry
  {
    sf::Packet packet;
    PlayerId target_pid{};
    TargetMode target_mode{};
    std::string title;
  };

  bool SetupNetSettings();
  std::optional<SaveSyncInfo> CollectSaveSyncInfo();
  bool SyncSaveData(const SaveSyncInfo& sync_info);
  bool SyncCodes();
  void CheckSyncAndStartGame();

  u64 GetInitialNetPlayRTC() const;

  template <typename... Data>
  void SendResponseToPlayer(const Client& player, const MessageID message_id,
                            Data&&... data_to_send);
  template <typename... Data>
  void SendResponseToAllPlayers(const MessageID message_id, Data&&... data_to_send);
  void SendToClients(const sf::Packet& packet, PlayerId skip_pid = 0,
                     u8 channel_id = DEFAULT_CHANNEL);
  void Send(ENetPeer* socket, const sf::Packet& packet, u8 channel_id = DEFAULT_CHANNEL);
  ConnectionError OnConnect(ENetPeer* socket, sf::Packet& received_packet);
  unsigned int OnDisconnect(const Client& player);
  unsigned int OnData(sf::Packet& packet, Client& player);

  void OnTraversalStateChanged() override;
  void OnConnectReady(ENetAddress) override {}
  void OnConnectFailed(Common::TraversalConnectFailedReason) override {}
  void OnTtlDetermined(u8 ttl) override;
  void UpdatePadMapping();
  void UpdateGBAConfig();
  void UpdateWiimoteMapping();
  std::vector<std::pair<std::string, std::string>> GetInterfaceListInternal() const;
  void ChunkedDataThreadFunc();
  void ChunkedDataSend(sf::Packet&& packet, PlayerId pid, const TargetMode target_mode);
  void ChunkedDataAbort();

  void SetupIndex();
  bool PlayerHasControllerMapped(PlayerId pid) const;

  // pulled from OnConnect()
  void AssignNewUserAPad(const Client& player);
  // pulled from OnConnect()
  // returns the PID given
  PlayerId GiveFirstAvailableIDTo(ENetPeer* player);

  NetSettings m_settings;

  bool m_is_running = false;
  bool m_do_loop = false;
  Common::Timer m_ping_timer;
  u32 m_ping_key = 0;
  bool m_update_pings = false;
  u32 m_current_game = 0;
  unsigned int m_target_buffer_size = 0;
  PadMappingArray m_pad_map;
  GBAConfigArray m_gba_config;
  PadMappingArray m_wiimote_map;
  unsigned int m_save_data_synced_players = 0;
  unsigned int m_codes_synced_players = 0;
  bool m_saves_synced = true;
  bool m_codes_synced = true;
  bool m_start_pending = false;
  bool m_host_input_authority = false;
  PlayerId m_current_golfer = 1;
  PlayerId m_pending_golfer = 0;

  std::map<PlayerId, Client> m_players;

  std::unordered_map<u32, std::vector<std::pair<PlayerId, u64>>> m_timebase_by_frame;
  bool m_desync_detected = false;

  struct
  {
    std::recursive_mutex game;
    // lock order
    std::recursive_mutex players;
    std::recursive_mutex async_queue_write;
    std::recursive_mutex chunked_data_queue_write;
  } m_crit;

  Common::SPSCQueue<AsyncQueueEntry, false> m_async_queue;
  Common::SPSCQueue<ChunkedDataQueueEntry, false> m_chunked_data_queue;

  SyncIdentifier m_selected_game_identifier;
  std::string m_selected_game_name;
  std::thread m_thread;
  Common::Event m_chunked_data_event;
  Common::Event m_chunked_data_complete_event;
  std::thread m_chunked_data_thread;
  u32 m_next_chunked_data_id = 0;
  std::unordered_map<u32, unsigned int> m_chunked_data_complete_count;
  bool m_abort_chunked_data = false;

  ENetHost* m_server = nullptr;
  Common::TraversalClient* m_traversal_client = nullptr;
  NetPlayUI* m_dialog = nullptr;
  NetPlayIndex m_index;
};
}  // namespace NetPlay
