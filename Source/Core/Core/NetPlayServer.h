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
#include "Common/QoSSession.h"
#include "Common/SPSCQueue.h"
#include "Common/Timer.h"
#include "Common/TraversalClient.h"
#include "Core/NetPlayProto.h"
#include "InputCommon/GCPadStatus.h"

namespace NetPlay
{
class NetPlayUI;
enum class PlayerGameStatus;

class NetPlayServer : public TraversalClientClient
{
public:
  void ThreadFunc();
  void SendAsyncToClients(sf::Packet&& packet);

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

  bool SyncSaveData();
  bool SyncCodes();
  void CheckSyncAndStartGame();
  bool CompressFileIntoPacket(const std::string& file_path, sf::Packet& packet);
  bool CompressBufferIntoPacket(const std::vector<u8>& in_buffer, sf::Packet& packet);
  void SendFirstReceivedToHost(PadIndex map, bool state);

  u64 GetInitialNetPlayRTC() const;

  void SendToClients(const sf::Packet& packet, const PlayerId skip_pid = 0);
  void Send(ENetPeer* socket, const sf::Packet& packet);
  unsigned int OnConnect(ENetPeer* socket);
  unsigned int OnDisconnect(const Client& player);
  unsigned int OnData(sf::Packet& packet, Client& player);

  void OnTraversalStateChanged() override;
  void OnConnectReady(ENetAddress) override {}
  void OnConnectFailed(u8) override {}
  void UpdatePadMapping();
  void UpdateWiimoteMapping();
  std::vector<std::pair<std::string, std::string>> GetInterfaceListInternal() const;

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

  std::map<PlayerId, Client> m_players;

  std::unordered_map<u32, std::vector<std::pair<PlayerId, u64>>> m_timebase_by_frame;
  bool m_desync_detected;

  std::array<GCPadStatus, 4> m_last_pad_status{};
  std::array<bool, 4> m_first_pad_status_received{};

  struct
  {
    std::recursive_mutex game;
    // lock order
    std::recursive_mutex players;
    std::recursive_mutex async_queue_write;
  } m_crit;

  std::string m_selected_game;
  std::thread m_thread;
  Common::SPSCQueue<sf::Packet, false> m_async_queue;

  ENetHost* m_server = nullptr;
  TraversalClient* m_traversal_client = nullptr;
  NetPlayUI* m_dialog = nullptr;
};
}  // namespace NetPlay
