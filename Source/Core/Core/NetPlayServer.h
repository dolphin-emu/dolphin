// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SFML/Network/Packet.hpp>
#include <map>
#include <memory>
#include <mutex>
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
#include "InputCommon/GCPadStatus.h"
#include "UICommon/NetPlayIndex.h"

namespace NetPlay
{
class NetPlayUI;
enum class PlayerGameStatus;

class NetPlayServer : public TraversalClientClient
{
public:
  void ThreadFunc();
  void SendAsync(sf::Packet&& packet, PlayerId pid, u8 channel_id = DEFAULT_CHANNEL);
  void SendAsyncToClients(sf::Packet&& packet, PlayerId skip_pid = 0,
                          u8 channel_id = DEFAULT_CHANNEL);
  void SendChunked(sf::Packet&& packet, PlayerId pid, const std::string& title = "");
  void SendChunkedToClients(sf::Packet&& packet, PlayerId skip_pid = 0,
                            const std::string& title = "");

  NetPlayServer(u16 port, bool forward_port, const NetTraversalConfig& traversal_config);
  ~NetPlayServer();

  bool ChangeGame(const std::string& game);
  bool ComputeMD5(const std::string& file_identifier);
  bool AbortMD5();
  void SendChatMessage(const std::string& msg);

  void SetNetSettings(const NetSettings& settings);

  bool DoAllPlayersHaveIPLDump() const;
  bool StartGame();
  bool RequestStartGame();
  void AbortGameStart();

  PadMappingArray GetPadMapping() const;
  void SetPadMapping(const PadMappingArray& mappings);

  PadMappingArray GetWiimoteMapping() const;
  void SetWiimoteMapping(const PadMappingArray& mappings);

  void AdjustPadBufferSize(unsigned int size);
  void SetHostInputAuthority(bool enable);

  void KickPlayer(PlayerId player);

  u16 GetPort() const;

  void SetNetPlayUI(NetPlayUI* dialog);
  std::unordered_set<std::string> GetInterfaceSet() const;
  std::string GetInterfaceHost(const std::string& inter) const;

  bool is_connected = false;

private:
  class Client
  {
  public:
    PlayerId pid;
    std::string name;
    std::string revision;
    PlayerGameStatus game_status;
    bool has_ipl_dump;

    ENetPeer* socket;
    u32 ping;
    u32 current_game;

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
    PlayerId target_pid;
    TargetMode target_mode;
    u8 channel_id;
  };

  struct ChunkedDataQueueEntry
  {
    sf::Packet packet;
    PlayerId target_pid;
    TargetMode target_mode;
    std::string title;
  };

  bool SyncSaveData();
  bool SyncCodes();
  void CheckSyncAndStartGame();
  bool CompressFileIntoPacket(const std::string& file_path, sf::Packet& packet);
  bool CompressBufferIntoPacket(const std::vector<u8>& in_buffer, sf::Packet& packet);

  u64 GetInitialNetPlayRTC() const;

  void SendToClients(const sf::Packet& packet, PlayerId skip_pid = 0,
                     u8 channel_id = DEFAULT_CHANNEL);
  void Send(ENetPeer* socket, const sf::Packet& packet, u8 channel_id = DEFAULT_CHANNEL);
  unsigned int OnConnect(ENetPeer* socket, sf::Packet& rpac);
  unsigned int OnDisconnect(const Client& player);
  unsigned int OnData(sf::Packet& packet, Client& player);

  void OnTraversalStateChanged() override;
  void OnConnectReady(ENetAddress) override {}
  void OnConnectFailed(u8) override {}
  void UpdatePadMapping();
  void UpdateWiimoteMapping();
  std::vector<std::pair<std::string, std::string>> GetInterfaceListInternal() const;
  void ChunkedDataThreadFunc();
  void ChunkedDataSend(sf::Packet&& packet, PlayerId pid, const TargetMode target_mode);
  void ChunkedDataAbort();

  void SetupIndex();
  bool PlayerHasControllerMapped(PlayerId pid) const;

  NetSettings m_settings;

  bool m_is_running = false;
  bool m_do_loop = false;
  Common::Timer m_ping_timer;
  u32 m_ping_key = 0;
  bool m_update_pings = false;
  u32 m_current_game = 0;
  unsigned int m_target_buffer_size = 0;
  PadMappingArray m_pad_map;
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
  bool m_desync_detected;

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

  std::string m_selected_game;
  std::thread m_thread;
  Common::Event m_chunked_data_event;
  Common::Event m_chunked_data_complete_event;
  std::thread m_chunked_data_thread;
  u32 m_next_chunked_data_id;
  std::unordered_map<u32, unsigned int> m_chunked_data_complete_count;
  bool m_abort_chunked_data = false;

  ENetHost* m_server = nullptr;
  TraversalClient* m_traversal_client = nullptr;
  NetPlayUI* m_dialog = nullptr;
  NetPlayIndex m_index;
};
}  // namespace NetPlay
