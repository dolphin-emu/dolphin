// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/NetPlayServer.h"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <lzo/lzo1x.h>

#include "Common/CommonPaths.h"
#include "Common/ENetUtil.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/SFMLHelper.h"
#include "Common/StringUtil.h"
#include "Common/UPnP.h"
#include "Common/Version.h"
#include "Core/ActionReplay.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/HW/GCMemcard/GCMemcardDirectory.h"
#include "Core/HW/GCMemcard/GCMemcardRaw.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiiSave.h"
#include "Core/HW/WiiSaveStructs.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/Uids.h"
#include "Core/NetPlayClient.h"  //for NetPlayUI
#include "DiscIO/Enums.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"
#include "UICommon/GameFile.h"

#if !defined(_WIN32)
#include <sys/socket.h>
#include <sys/types.h>
#ifdef __HAIKU__
#define _BSD_SOURCE
#include <bsd/ifaddrs.h>
#elif !defined ANDROID
#include <ifaddrs.h>
#endif
#include <arpa/inet.h>
#endif

namespace NetPlay
{
NetPlayServer::~NetPlayServer()
{
  if (is_connected)
  {
    m_do_loop = false;
    m_chunked_data_event.Set();
    m_chunked_data_complete_event.Set();
    if (m_chunked_data_thread.joinable())
      m_chunked_data_thread.join();
    m_thread.join();
    enet_host_destroy(m_server);

    if (g_MainNetHost.get() == m_server)
    {
      g_MainNetHost.release();
    }

    if (m_traversal_client)
    {
      g_TraversalClient->m_Client = nullptr;
      ReleaseTraversalClient();
    }
  }

#ifdef USE_UPNP
  UPnP::StopPortmapping();
#endif
}

// called from ---GUI--- thread
NetPlayServer::NetPlayServer(const u16 port, const bool forward_port,
                             const NetTraversalConfig& traversal_config)
{
  //--use server time
  if (enet_initialize() != 0)
  {
    PanicAlertT("Enet Didn't Initialize");
  }

  m_pad_map.fill(0);
  m_wiimote_map.fill(0);

  if (traversal_config.use_traversal)
  {
    if (!EnsureTraversalClient(traversal_config.traversal_host, traversal_config.traversal_port,
                               port))
      return;

    g_TraversalClient->m_Client = this;
    m_traversal_client = g_TraversalClient.get();

    m_server = g_MainNetHost.get();

    if (g_TraversalClient->GetState() == TraversalClient::Failure)
      g_TraversalClient->ReconnectToServer();
  }
  else
  {
    ENetAddress serverAddr;
    serverAddr.host = ENET_HOST_ANY;
    serverAddr.port = port;
    m_server = enet_host_create(&serverAddr, 10, CHANNEL_COUNT, 0, 0);
    if (m_server != nullptr)
      m_server->intercept = ENetUtil::InterceptCallback;

    SetupIndex();
  }
  if (m_server != nullptr)
  {
    is_connected = true;
    m_do_loop = true;
    m_thread = std::thread(&NetPlayServer::ThreadFunc, this);
    m_target_buffer_size = 5;
    m_chunked_data_thread = std::thread(&NetPlayServer::ChunkedDataThreadFunc, this);

#ifdef USE_UPNP
    if (forward_port)
      UPnP::TryPortmapping(port);
#endif
  }
}

static PlayerId* PeerPlayerId(ENetPeer* peer)
{
  return static_cast<PlayerId*>(peer->data);
}

static void ClearPeerPlayerId(ENetPeer* peer)
{
  if (peer->data)
  {
    delete PeerPlayerId(peer);
    peer->data = nullptr;
  }
}

void NetPlayServer::SetupIndex()
{
  if (!Config::Get(Config::NETPLAY_USE_INDEX))
    return;

  NetPlaySession session;

  session.name = Config::Get(Config::NETPLAY_INDEX_NAME);
  session.region = Config::Get(Config::NETPLAY_INDEX_REGION);
  session.has_password = !Config::Get(Config::NETPLAY_INDEX_PASSWORD).empty();
  session.method = m_traversal_client ? "traversal" : "direct";
  session.game_id = m_selected_game.empty() ? "UNKNOWN" : m_selected_game;
  session.player_count = static_cast<int>(m_players.size());
  session.in_game = m_is_running;
  session.port = GetPort();

  if (m_traversal_client)
  {
    if (m_traversal_client->GetState() != TraversalClient::Connected)
      return;

    session.server_id = std::string(g_TraversalClient->GetHostID().data(), 8);
  }
  else
  {
    Common::HttpRequest request;
    // ENet does not support IPv6, so IPv4 has to be used
    request.UseIPv4();
    Common::HttpRequest::Response response =
        request.Get("https://ip.dolphin-emu.org/", {{"X-Is-Dolphin", "1"}});

    if (!response.has_value())
      return;

    session.server_id = std::string(response->begin(), response->end());
  }

  session.EncryptID(Config::Get(Config::NETPLAY_INDEX_PASSWORD));

  if (m_dialog != nullptr)
  {
    bool success = m_index.Add(session);
    m_dialog->OnIndexAdded(success, success ? "" : m_index.GetLastError());
  }

  m_index.SetErrorCallback([this] {
    if (m_dialog != nullptr)
      m_dialog->OnIndexRefreshFailed(m_index.GetLastError());
  });
}

// called from ---NETPLAY--- thread
void NetPlayServer::ThreadFunc()
{
  while (m_do_loop)
  {
    // update pings every so many seconds
    if ((m_ping_timer.GetTimeElapsed() > 1000) || m_update_pings)
    {
      m_ping_key = Common::Timer::GetTimeMs();

      sf::Packet spac;
      spac << (MessageId)NP_MSG_PING;
      spac << m_ping_key;

      m_ping_timer.Start();
      SendToClients(spac);

      m_index.SetPlayerCount(static_cast<int>(m_players.size()));
      m_index.SetGame(m_selected_game);
      m_index.SetInGame(m_is_running);

      m_update_pings = false;
    }

    ENetEvent netEvent;
    int net;
    if (m_traversal_client)
      m_traversal_client->HandleResends();
    net = enet_host_service(m_server, &netEvent, 1000);
    while (!m_async_queue.Empty())
    {
      {
        std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
        auto& e = m_async_queue.Front();
        if (e.target_mode == TargetMode::Only)
        {
          if (m_players.find(e.target_pid) != m_players.end())
            Send(m_players.at(e.target_pid).socket, e.packet, e.channel_id);
        }
        else
        {
          SendToClients(e.packet, e.target_pid, e.channel_id);
        }
      }
      m_async_queue.Pop();
    }
    if (net > 0)
    {
      switch (netEvent.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
      {
        // Actual client initialization is deferred to the receive event, so here
        // we'll just log the new connection.
        INFO_LOG(NETPLAY, "Peer connected from: %x:%u", netEvent.peer->address.host,
                 netEvent.peer->address.port);
      }
      break;
      case ENET_EVENT_TYPE_RECEIVE:
      {
        sf::Packet rpac;
        rpac.append(netEvent.packet->data, netEvent.packet->dataLength);

        if (!netEvent.peer->data)
        {
          // uninitialized client, we'll assume this is their initialization packet
          unsigned int error;
          {
            std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
            error = OnConnect(netEvent.peer, rpac);
          }

          if (error)
          {
            sf::Packet spac;
            spac << static_cast<MessageId>(error);
            // don't need to lock, this client isn't in the client map
            Send(netEvent.peer, spac);

            ClearPeerPlayerId(netEvent.peer);
            enet_peer_disconnect_later(netEvent.peer, 0);
          }
        }
        else
        {
          auto it = m_players.find(*PeerPlayerId(netEvent.peer));
          Client& client = it->second;
          if (OnData(rpac, client) != 0)
          {
            // if a bad packet is received, disconnect the client
            std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
            OnDisconnect(client);

            ClearPeerPlayerId(netEvent.peer);
          }
        }
        enet_packet_destroy(netEvent.packet);
      }
      break;
      case ENET_EVENT_TYPE_DISCONNECT:
      {
        std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
        if (!netEvent.peer->data)
          break;
        auto it = m_players.find(*PeerPlayerId(netEvent.peer));
        if (it != m_players.end())
        {
          Client& client = it->second;
          OnDisconnect(client);

          ClearPeerPlayerId(netEvent.peer);
        }
      }
      break;
      default:
        break;
      }
    }
  }

  // close listening socket and client sockets
  for (auto& player_entry : m_players)
  {
    ClearPeerPlayerId(player_entry.second.socket);
    enet_peer_disconnect(player_entry.second.socket, 0);
  }
}  // namespace NetPlay

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnConnect(ENetPeer* socket, sf::Packet& rpac)
{
  // give new client first available id
  PlayerId pid = 1;
  for (auto i = m_players.begin(); i != m_players.end(); ++i)
  {
    if (i->second.pid == pid)
    {
      pid++;
      i = m_players.begin();
    }
  }
  socket->data = new PlayerId(pid);

  std::string npver;
  rpac >> npver;
  // Dolphin netplay version
  if (npver != Common::scm_rev_git_str)
    return CON_ERR_VERSION_MISMATCH;

  // game is currently running or game start is pending
  if (m_is_running || m_start_pending)
    return CON_ERR_GAME_RUNNING;

  // too many players
  if (m_players.size() >= 255)
    return CON_ERR_SERVER_FULL;

  // cause pings to be updated
  m_update_pings = true;

  Client player;
  player.pid = pid;
  player.socket = socket;

  rpac >> player.revision;
  rpac >> player.name;

  // try to automatically assign new user a pad
  for (PlayerId& mapping : m_pad_map)
  {
    if (mapping == 0)
    {
      mapping = player.pid;
      break;
    }
  }

  // send join message to already connected clients
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_PLAYER_JOIN);
  spac << player.pid << player.name << player.revision;
  SendToClients(spac);

  // send new client success message with their id
  spac.clear();
  spac << static_cast<MessageId>(0);
  spac << player.pid;
  Send(player.socket, spac);

  // send new client the selected game
  if (!m_selected_game.empty())
  {
    spac.clear();
    spac << static_cast<MessageId>(NP_MSG_CHANGE_GAME);
    spac << m_selected_game;
    Send(player.socket, spac);
  }

  if (!m_host_input_authority)
  {
    // send the pad buffer value
    spac.clear();
    spac << static_cast<MessageId>(NP_MSG_PAD_BUFFER);
    spac << static_cast<u32>(m_target_buffer_size);
    Send(player.socket, spac);
  }

  // send input authority state
  spac.clear();
  spac << static_cast<MessageId>(NP_MSG_HOST_INPUT_AUTHORITY);
  spac << m_host_input_authority;
  Send(player.socket, spac);

  // sync GC SRAM with new client
  if (!g_SRAM_netplay_initialized)
  {
    SConfig::GetInstance().m_strSRAM = File::GetUserPath(F_GCSRAM_IDX);
    InitSRAM();
    g_SRAM_netplay_initialized = true;
  }
  spac.clear();
  spac << static_cast<MessageId>(NP_MSG_SYNC_GC_SRAM);
  for (size_t i = 0; i < sizeof(g_SRAM) - offsetof(Sram, settings); ++i)
  {
    spac << g_SRAM[offsetof(Sram, settings) + i];
  }
  Send(player.socket, spac);

  // sync values with new client
  for (const auto& p : m_players)
  {
    spac.clear();
    spac << static_cast<MessageId>(NP_MSG_PLAYER_JOIN);
    spac << p.second.pid << p.second.name << p.second.revision;
    Send(player.socket, spac);

    spac.clear();
    spac << static_cast<MessageId>(NP_MSG_GAME_STATUS);
    spac << p.second.pid << static_cast<u32>(p.second.game_status);
    Send(player.socket, spac);
  }

  if (Config::Get(Config::NETPLAY_ENABLE_QOS))
    player.qos_session = Common::QoSSession(player.socket);

  // add client to the player list
  {
    std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
    m_players.emplace(*PeerPlayerId(player.socket), std::move(player));
    UpdatePadMapping();  // sync pad mappings with everyone
    UpdateWiimoteMapping();
  }

  return 0;
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnDisconnect(const Client& player)
{
  const PlayerId pid = player.pid;

  if (m_is_running)
  {
    for (PlayerId& mapping : m_pad_map)
    {
      if (mapping == pid && pid != 1)
      {
        std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
        m_is_running = false;

        sf::Packet spac;
        spac << (MessageId)NP_MSG_DISABLE_GAME;
        // this thread doesn't need players lock
        SendToClients(spac);
        break;
      }
    }
  }

  if (m_start_pending)
  {
    ChunkedDataAbort();
    m_dialog->OnGameStartAborted();
    m_start_pending = false;
  }

  sf::Packet spac;
  spac << (MessageId)NP_MSG_PLAYER_LEAVE;
  spac << pid;

  enet_peer_disconnect(player.socket, 0);

  std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
  auto it = m_players.find(player.pid);
  if (it != m_players.end())
    m_players.erase(it);

  // alert other players of disconnect
  SendToClients(spac);

  for (PlayerId& mapping : m_pad_map)
  {
    if (mapping == pid)
    {
      mapping = 0;
      UpdatePadMapping();
    }
  }

  for (PlayerId& mapping : m_wiimote_map)
  {
    if (mapping == pid)
    {
      mapping = 0;
      UpdateWiimoteMapping();
    }
  }

  return 0;
}

// called from ---GUI--- thread
PadMappingArray NetPlayServer::GetPadMapping() const
{
  return m_pad_map;
}

PadMappingArray NetPlayServer::GetWiimoteMapping() const
{
  return m_wiimote_map;
}

// called from ---GUI--- thread
void NetPlayServer::SetPadMapping(const PadMappingArray& mappings)
{
  m_pad_map = mappings;
  UpdatePadMapping();
}

// called from ---GUI--- thread
void NetPlayServer::SetWiimoteMapping(const PadMappingArray& mappings)
{
  m_wiimote_map = mappings;
  UpdateWiimoteMapping();
}

// called from ---GUI--- thread and ---NETPLAY--- thread
void NetPlayServer::UpdatePadMapping()
{
  sf::Packet spac;
  spac << (MessageId)NP_MSG_PAD_MAPPING;
  for (PlayerId mapping : m_pad_map)
  {
    spac << mapping;
  }
  SendToClients(spac);
}

// called from ---NETPLAY--- thread
void NetPlayServer::UpdateWiimoteMapping()
{
  sf::Packet spac;
  spac << (MessageId)NP_MSG_WIIMOTE_MAPPING;
  for (PlayerId mapping : m_wiimote_map)
  {
    spac << mapping;
  }
  SendToClients(spac);
}

// called from ---GUI--- thread and ---NETPLAY--- thread
void NetPlayServer::AdjustPadBufferSize(unsigned int size)
{
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

  m_target_buffer_size = size;

  // not needed on clients with host input authority
  if (!m_host_input_authority)
  {
    // tell clients to change buffer size
    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_PAD_BUFFER);
    spac << static_cast<u32>(m_target_buffer_size);

    SendAsyncToClients(std::move(spac));
  }
}

void NetPlayServer::SetHostInputAuthority(const bool enable)
{
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

  m_host_input_authority = enable;

  // tell clients about the new value
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_HOST_INPUT_AUTHORITY);
  spac << m_host_input_authority;

  SendAsyncToClients(std::move(spac));

  // resend pad buffer to clients when disabled
  if (!m_host_input_authority)
    AdjustPadBufferSize(m_target_buffer_size);
}

void NetPlayServer::SendAsync(sf::Packet&& packet, const PlayerId pid, const u8 channel_id)
{
  {
    std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
    m_async_queue.Push(AsyncQueueEntry{std::move(packet), pid, TargetMode::Only, channel_id});
  }
  ENetUtil::WakeupThread(m_server);
}

void NetPlayServer::SendAsyncToClients(sf::Packet&& packet, const PlayerId skip_pid,
                                       const u8 channel_id)
{
  {
    std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
    m_async_queue.Push(
        AsyncQueueEntry{std::move(packet), skip_pid, TargetMode::AllExcept, channel_id});
  }
  ENetUtil::WakeupThread(m_server);
}

void NetPlayServer::SendChunked(sf::Packet&& packet, const PlayerId pid, const std::string& title)
{
  {
    std::lock_guard<std::recursive_mutex> lkq(m_crit.chunked_data_queue_write);
    m_chunked_data_queue.Push(
        ChunkedDataQueueEntry{std::move(packet), pid, TargetMode::Only, title});
  }
  m_chunked_data_event.Set();
}

void NetPlayServer::SendChunkedToClients(sf::Packet&& packet, const PlayerId skip_pid,
                                         const std::string& title)
{
  {
    std::lock_guard<std::recursive_mutex> lkq(m_crit.chunked_data_queue_write);
    m_chunked_data_queue.Push(
        ChunkedDataQueueEntry{std::move(packet), skip_pid, TargetMode::AllExcept, title});
  }
  m_chunked_data_event.Set();
}

// called from ---NETPLAY--- thread
unsigned int NetPlayServer::OnData(sf::Packet& packet, Client& player)
{
  MessageId mid;
  packet >> mid;

  INFO_LOG(NETPLAY, "Got client message: %x", mid);

  // don't need lock because this is the only thread that modifies the players
  // only need locks for writes to m_players in this thread

  switch (mid)
  {
  case NP_MSG_CHAT_MESSAGE:
  {
    std::string msg;
    packet >> msg;

    // send msg to other clients
    sf::Packet spac;
    spac << (MessageId)NP_MSG_CHAT_MESSAGE;
    spac << player.pid;
    spac << msg;

    SendToClients(spac, player.pid);
  }
  break;

  case NP_MSG_CHUNKED_DATA_PROGRESS:
  {
    u32 cid;
    packet >> cid;
    u64 progress = Common::PacketReadU64(packet);

    m_dialog->SetChunkedProgress(player.pid, progress);
  }
  break;

  case NP_MSG_CHUNKED_DATA_COMPLETE:
  {
    u32 cid;
    packet >> cid;

    if (m_chunked_data_complete_count.find(cid) != m_chunked_data_complete_count.end())
    {
      m_chunked_data_complete_count[cid]++;
      m_chunked_data_complete_event.Set();
    }
  }
  break;

  case NP_MSG_PAD_DATA:
  {
    // if this is pad data from the last game still being received, ignore it
    if (player.current_game != m_current_game)
      break;

    sf::Packet spac;
    spac << static_cast<MessageId>(m_host_input_authority ? NP_MSG_PAD_HOST_DATA : NP_MSG_PAD_DATA);

    while (!packet.endOfPacket())
    {
      PadIndex map;
      packet >> map;

      // If the data is not from the correct player,
      // then disconnect them.
      if (m_pad_map.at(map) != player.pid)
      {
        return 1;
      }

      GCPadStatus pad;
      packet >> pad.button >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >>
          pad.substickX >> pad.substickY >> pad.triggerLeft >> pad.triggerRight >> pad.isConnected;

      spac << map << pad.button << pad.analogA << pad.analogB << pad.stickX << pad.stickY
           << pad.substickX << pad.substickY << pad.triggerLeft << pad.triggerRight
           << pad.isConnected;
    }

    if (m_host_input_authority)
    {
      // Prevent crash before game stop if the golfer disconnects
      if (m_current_golfer != 0 && m_players.find(m_current_golfer) != m_players.end())
        Send(m_players.at(m_current_golfer).socket, spac);
    }
    else
    {
      SendToClients(spac, player.pid);
    }
  }
  break;

  case NP_MSG_PAD_HOST_DATA:
  {
    // Kick player if they're not the golfer.
    if (m_current_golfer != 0 && player.pid != m_current_golfer)
      return 1;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_PAD_DATA);

    while (!packet.endOfPacket())
    {
      PadIndex map;
      packet >> map;

      GCPadStatus pad;
      packet >> pad.button >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >>
          pad.substickX >> pad.substickY >> pad.triggerLeft >> pad.triggerRight >> pad.isConnected;

      spac << map << pad.button << pad.analogA << pad.analogB << pad.stickX << pad.stickY
           << pad.substickX << pad.substickY << pad.triggerLeft << pad.triggerRight
           << pad.isConnected;
    }

    SendToClients(spac, player.pid);
  }
  break;

  case NP_MSG_WIIMOTE_DATA:
  {
    // if this is Wiimote data from the last game still being received, ignore it
    if (player.current_game != m_current_game)
      break;

    PadIndex map;
    u8 size;
    packet >> map >> size;
    std::vector<u8> data(size);
    for (size_t i = 0; i < data.size(); ++i)
      packet >> data[i];

    // If the data is not from the correct player,
    // then disconnect them.
    if (m_wiimote_map.at(map) != player.pid)
    {
      return 1;
    }

    // relay to clients
    sf::Packet spac;
    spac << (MessageId)NP_MSG_WIIMOTE_DATA;
    spac << map;
    spac << size;
    for (const u8& byte : data)
      spac << byte;

    SendToClients(spac, player.pid);
  }
  break;

  case NP_MSG_GOLF_REQUEST:
  {
    PlayerId pid;
    packet >> pid;

    // Check if player ID is valid and sender isn't a spectator
    if (!m_players.count(pid) || !PlayerHasControllerMapped(player.pid))
      break;

    if (m_host_input_authority && m_settings.m_GolfMode && m_pending_golfer == 0 &&
        m_current_golfer != pid && PlayerHasControllerMapped(pid))
    {
      m_pending_golfer = pid;

      sf::Packet spac;
      spac << static_cast<MessageId>(NP_MSG_GOLF_PREPARE);
      Send(m_players[pid].socket, spac);
    }
  }
  break;

  case NP_MSG_GOLF_RELEASE:
  {
    if (m_pending_golfer == 0)
      break;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_GOLF_SWITCH);
    spac << static_cast<PlayerId>(m_pending_golfer);
    SendToClients(spac);
  }
  break;

  case NP_MSG_GOLF_ACQUIRE:
  {
    if (m_pending_golfer == 0)
      break;

    m_current_golfer = m_pending_golfer;
    m_pending_golfer = 0;
  }
  break;

  case NP_MSG_GOLF_PREPARE:
  {
    if (m_pending_golfer == 0)
      break;

    m_current_golfer = 0;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_GOLF_SWITCH);
    spac << static_cast<PlayerId>(0);
    SendToClients(spac);
  }
  break;

  case NP_MSG_PONG:
  {
    const u32 ping = (u32)m_ping_timer.GetTimeElapsed();
    u32 ping_key = 0;
    packet >> ping_key;

    if (m_ping_key == ping_key)
    {
      player.ping = ping;
    }

    sf::Packet spac;
    spac << (MessageId)NP_MSG_PLAYER_PING_DATA;
    spac << player.pid;
    spac << player.ping;

    SendToClients(spac);
  }
  break;

  case NP_MSG_START_GAME:
  {
    packet >> player.current_game;
  }
  break;

  case NP_MSG_STOP_GAME:
  {
    if (!m_is_running)
      break;

    m_is_running = false;

    // tell clients to stop game
    sf::Packet spac;
    spac << (MessageId)NP_MSG_STOP_GAME;

    std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
    SendToClients(spac);
  }
  break;

  case NP_MSG_GAME_STATUS:
  {
    u32 status;
    packet >> status;

    m_players[player.pid].game_status = static_cast<PlayerGameStatus>(status);

    // send msg to other clients
    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_GAME_STATUS);
    spac << player.pid;
    spac << status;

    SendToClients(spac);
  }
  break;

  case NP_MSG_IPL_STATUS:
  {
    bool status;
    packet >> status;

    m_players[player.pid].has_ipl_dump = status;
  }
  break;

  case NP_MSG_POWER_BUTTON:
  {
    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_POWER_BUTTON);
    SendToClients(spac, player.pid);
  }
  break;

  case NP_MSG_TIMEBASE:
  {
    u64 timebase = Common::PacketReadU64(packet);
    u32 frame;
    packet >> frame;

    if (m_desync_detected)
      break;

    std::vector<std::pair<PlayerId, u64>>& timebases = m_timebase_by_frame[frame];
    timebases.emplace_back(player.pid, timebase);
    if (timebases.size() >= m_players.size())
    {
      // we have all records for this frame

      if (!std::all_of(timebases.begin(), timebases.end(), [&](std::pair<PlayerId, u64> pair) {
            return pair.second == timebases[0].second;
          }))
      {
        int pid_to_blame = 0;
        for (auto pair : timebases)
        {
          if (std::all_of(timebases.begin(), timebases.end(), [&](std::pair<PlayerId, u64> other) {
                return other.first == pair.first || other.second != pair.second;
              }))
          {
            // we are the only outlier
            pid_to_blame = pair.first;
            break;
          }
        }

        sf::Packet spac;
        spac << (MessageId)NP_MSG_DESYNC_DETECTED;
        spac << pid_to_blame;
        spac << frame;
        SendToClients(spac);

        m_desync_detected = true;
      }
      m_timebase_by_frame.erase(frame);
    }
  }
  break;

  case NP_MSG_MD5_PROGRESS:
  {
    int progress;
    packet >> progress;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_MD5_PROGRESS);
    spac << player.pid;
    spac << progress;

    SendToClients(spac);
  }
  break;

  case NP_MSG_MD5_RESULT:
  {
    std::string result;
    packet >> result;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_MD5_RESULT);
    spac << player.pid;
    spac << result;

    SendToClients(spac);
  }
  break;

  case NP_MSG_MD5_ERROR:
  {
    std::string error;
    packet >> error;

    sf::Packet spac;
    spac << static_cast<MessageId>(NP_MSG_MD5_ERROR);
    spac << player.pid;
    spac << error;

    SendToClients(spac);
  }
  break;

  case NP_MSG_SYNC_SAVE_DATA:
  {
    MessageId sub_id;
    packet >> sub_id;

    switch (sub_id)
    {
    case SYNC_SAVE_DATA_SUCCESS:
    {
      if (m_start_pending)
      {
        m_save_data_synced_players++;
        if (m_save_data_synced_players >= m_players.size() - 1)
        {
          m_dialog->AppendChat(Common::GetStringT("All players' saves synchronized."));

          // Saves are synced, check if codes are as well and attempt to start the game
          m_saves_synced = true;
          CheckSyncAndStartGame();
        }
      }
    }
    break;

    case SYNC_SAVE_DATA_FAILURE:
    {
      m_dialog->AppendChat(StringFromFormat(Common::GetStringT("%s failed to synchronize.").c_str(),
                                            player.name.c_str()));
      m_dialog->OnGameStartAborted();
      ChunkedDataAbort();
      m_start_pending = false;
    }
    break;

    default:
      PanicAlertT(
          "Unknown SYNC_SAVE_DATA message with id:%d received from player:%d Kicking player!",
          sub_id, player.pid);
      return 1;
    }
  }
  break;

  case NP_MSG_SYNC_CODES:
  {
    // Receive Status of Code Sync
    MessageId sub_id;
    packet >> sub_id;

    // Check If Code Sync was successful or not
    switch (sub_id)
    {
    case SYNC_CODES_SUCCESS:
    {
      if (m_start_pending)
      {
        if (++m_codes_synced_players >= m_players.size() - 1)
        {
          m_dialog->AppendChat(Common::GetStringT("All players' codes synchronized."));

          // Codes are synced, check if saves are as well and attempt to start the game
          m_codes_synced = true;
          CheckSyncAndStartGame();
        }
      }
    }
    break;

    case SYNC_CODES_FAILURE:
    {
      m_dialog->AppendChat(StringFromFormat(
          Common::GetStringT("%s failed to synchronize codes.").c_str(), player.name.c_str()));
      m_dialog->OnGameStartAborted();
      m_start_pending = false;
    }
    break;

    default:
      PanicAlertT(
          "Unknown SYNC_GECKO_CODES message with id:%d received from player:%d Kicking player!",
          sub_id, player.pid);
      return 1;
    }
  }
  break;

  default:
    PanicAlertT("Unknown message with id:%d received from player:%d Kicking player!", mid,
                player.pid);
    // unknown message, kick the client
    return 1;
  }

  return 0;
}

void NetPlayServer::OnTraversalStateChanged()
{
  const TraversalClient::State state = m_traversal_client->GetState();

  if (g_TraversalClient->GetHostID()[0] != '\0')
    SetupIndex();

  if (!m_dialog)
    return;

  if (state == TraversalClient::Failure)
    m_dialog->OnTraversalError(m_traversal_client->GetFailureReason());

  m_dialog->OnTraversalStateChanged(state);
}

// called from ---GUI--- thread
void NetPlayServer::SendChatMessage(const std::string& msg)
{
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_CHAT_MESSAGE);
  spac << static_cast<PlayerId>(0);  // server id always 0
  spac << msg;

  SendAsyncToClients(std::move(spac));
}

// called from ---GUI--- thread
bool NetPlayServer::ChangeGame(const std::string& game)
{
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);

  m_selected_game = game;

  // send changed game to clients
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_CHANGE_GAME);
  spac << game;

  SendAsyncToClients(std::move(spac));

  return true;
}

// called from ---GUI--- thread
bool NetPlayServer::ComputeMD5(const std::string& file_identifier)
{
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_COMPUTE_MD5);
  spac << file_identifier;

  SendAsyncToClients(std::move(spac));

  return true;
}

// called from ---GUI--- thread
bool NetPlayServer::AbortMD5()
{
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_MD5_ABORT);

  SendAsyncToClients(std::move(spac));

  return true;
}

// called from ---GUI--- thread
void NetPlayServer::SetNetSettings(const NetSettings& settings)
{
  m_settings = settings;
}

bool NetPlayServer::DoAllPlayersHaveIPLDump() const
{
  return std::all_of(m_players.begin(), m_players.end(),
                     [](const auto& p) { return p.second.has_ipl_dump; });
}

// called from ---GUI--- thread
bool NetPlayServer::RequestStartGame()
{
  bool start_now = true;

  if (m_settings.m_SyncSaveData && m_players.size() > 1)
  {
    start_now = false;
    m_start_pending = true;
    if (!SyncSaveData())
    {
      PanicAlertT("Error synchronizing save data!");
      m_start_pending = false;
      return false;
    }
  }

  // Check To Send Codes to Clients
  if (m_settings.m_SyncCodes && m_players.size() > 1)
  {
    start_now = false;
    m_start_pending = true;
    if (!SyncCodes())
    {
      PanicAlertT("Error synchronizing cheat codes!");
      m_start_pending = false;
      return false;
    }
  }

  if (start_now)
  {
    return StartGame();
  }

  return true;
}

// called from multiple threads
bool NetPlayServer::StartGame()
{
  m_timebase_by_frame.clear();
  m_desync_detected = false;
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
  m_current_game = Common::Timer::GetTimeMs();

  // no change, just update with clients
  if (!m_host_input_authority)
    AdjustPadBufferSize(m_target_buffer_size);

  m_current_golfer = 1;
  m_pending_golfer = 0;

  const sf::Uint64 initial_rtc = GetInitialNetPlayRTC();

  const std::string region = SConfig::GetDirectoryForRegion(
      SConfig::ToGameCubeRegion(m_dialog->FindGameFile(m_selected_game)->GetRegion()));

  // tell clients to start game
  sf::Packet spac;
  spac << static_cast<MessageId>(NP_MSG_START_GAME);
  spac << m_current_game;
  spac << m_settings.m_CPUthread;
  spac << static_cast<std::underlying_type_t<PowerPC::CPUCore>>(m_settings.m_CPUcore);
  spac << m_settings.m_EnableCheats;
  spac << m_settings.m_SelectedLanguage;
  spac << m_settings.m_OverrideGCLanguage;
  spac << m_settings.m_ProgressiveScan;
  spac << m_settings.m_PAL60;
  spac << m_settings.m_DSPEnableJIT;
  spac << m_settings.m_DSPHLE;
  spac << m_settings.m_WriteToMemcard;
  spac << m_settings.m_CopyWiiSave;
  spac << m_settings.m_OCEnable;
  spac << m_settings.m_OCFactor;
  spac << m_settings.m_ReducePollingRate;

  for (auto& device : m_settings.m_EXIDevice)
    spac << device;

  spac << m_settings.m_EFBAccessEnable;
  spac << m_settings.m_BBoxEnable;
  spac << m_settings.m_ForceProgressive;
  spac << m_settings.m_EFBToTextureEnable;
  spac << m_settings.m_XFBToTextureEnable;
  spac << m_settings.m_DisableCopyToVRAM;
  spac << m_settings.m_ImmediateXFBEnable;
  spac << m_settings.m_EFBEmulateFormatChanges;
  spac << m_settings.m_SafeTextureCacheColorSamples;
  spac << m_settings.m_PerfQueriesEnable;
  spac << m_settings.m_FPRF;
  spac << m_settings.m_AccurateNaNs;
  spac << m_settings.m_SyncOnSkipIdle;
  spac << m_settings.m_SyncGPU;
  spac << m_settings.m_SyncGpuMaxDistance;
  spac << m_settings.m_SyncGpuMinDistance;
  spac << m_settings.m_SyncGpuOverclock;
  spac << m_settings.m_JITFollowBranch;
  spac << m_settings.m_FastDiscSpeed;
  spac << m_settings.m_MMU;
  spac << m_settings.m_Fastmem;
  spac << m_settings.m_SkipIPL;
  spac << m_settings.m_LoadIPLDump;
  spac << m_settings.m_VertexRounding;
  spac << m_settings.m_InternalResolution;
  spac << m_settings.m_EFBScaledCopy;
  spac << m_settings.m_FastDepthCalc;
  spac << m_settings.m_EnablePixelLighting;
  spac << m_settings.m_WidescreenHack;
  spac << m_settings.m_ForceFiltering;
  spac << m_settings.m_MaxAnisotropy;
  spac << m_settings.m_ForceTrueColor;
  spac << m_settings.m_DisableCopyFilter;
  spac << m_settings.m_DisableFog;
  spac << m_settings.m_ArbitraryMipmapDetection;
  spac << m_settings.m_ArbitraryMipmapDetectionThreshold;
  spac << m_settings.m_EnableGPUTextureDecoding;
  spac << m_settings.m_DeferEFBCopies;
  spac << m_settings.m_EFBAccessTileSize;
  spac << m_settings.m_EFBAccessDeferInvalidation;
  spac << m_settings.m_StrictSettingsSync;
  spac << initial_rtc;
  spac << m_settings.m_SyncSaveData;
  spac << region;
  spac << m_settings.m_SyncCodes;
  spac << m_settings.m_SyncAllWiiSaves;

  for (size_t i = 0; i < m_settings.m_WiimoteExtension.size(); i++)
  {
    const int extension =
        static_cast<ControllerEmu::Attachments*>(
            static_cast<WiimoteEmu::Wiimote*>(Wiimote::GetConfig()->GetController(int(i)))
                ->GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Attachments))
            ->GetSelectedAttachment();
    spac << extension;
  }

  spac << m_settings.m_GolfMode;

  SendAsyncToClients(std::move(spac));

  m_start_pending = false;
  m_is_running = true;

  return true;
}

void NetPlayServer::AbortGameStart()
{
  if (m_start_pending)
  {
    m_dialog->OnGameStartAborted();
    ChunkedDataAbort();
    m_start_pending = false;
  }
}

// called from ---GUI--- thread
bool NetPlayServer::SyncSaveData()
{
  // We're about to sync saves, so set m_saves_synced to false (waits to start game)
  m_saves_synced = false;

  m_save_data_synced_players = 0;

  u8 save_count = 0;

  constexpr size_t exi_device_count = 2;
  for (size_t i = 0; i < exi_device_count; i++)
  {
    if (m_settings.m_EXIDevice[i] == ExpansionInterface::EXIDEVICE_MEMORYCARD ||
        SConfig::GetInstance().m_EXIDevice[i] == ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER)
    {
      save_count++;
    }
  }

  const auto game = m_dialog->FindGameFile(m_selected_game);
  if (game == nullptr)
  {
    PanicAlertT("Selected game doesn't exist in game list!");
    return false;
  }

  bool wii_save = false;
  if (m_settings.m_CopyWiiSave && (game->GetPlatform() == DiscIO::Platform::WiiDisc ||
                                   game->GetPlatform() == DiscIO::Platform::WiiWAD ||
                                   game->GetPlatform() == DiscIO::Platform::ELFOrDOL))
  {
    wii_save = true;
    save_count++;
  }

  {
    sf::Packet pac;
    pac << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
    pac << static_cast<MessageId>(SYNC_SAVE_DATA_NOTIFY);
    pac << save_count;

    // send this on the chunked data channel to ensure it's sequenced properly
    SendAsyncToClients(std::move(pac), 0, CHUNKED_DATA_CHANNEL);
  }

  if (save_count == 0)
    return true;

  const std::string region =
      SConfig::GetDirectoryForRegion(SConfig::ToGameCubeRegion(game->GetRegion()));

  for (size_t i = 0; i < exi_device_count; i++)
  {
    const bool is_slot_a = i == 0;

    if (m_settings.m_EXIDevice[i] == ExpansionInterface::EXIDEVICE_MEMORYCARD)
    {
      std::string path = is_slot_a ? Config::Get(Config::MAIN_MEMCARD_A_PATH) :
                                     Config::Get(Config::MAIN_MEMCARD_B_PATH);

      MemoryCard::CheckPath(path, region, is_slot_a);

      bool mc251;
      IniFile gameIni = SConfig::LoadGameIni(game->GetGameID(), game->GetRevision());
      gameIni.GetOrCreateSection("Core")->Get("MemoryCard251", &mc251, false);

      if (mc251)
        path.insert(path.find_last_of('.'), ".251");

      sf::Packet pac;
      pac << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
      pac << static_cast<MessageId>(SYNC_SAVE_DATA_RAW);
      pac << is_slot_a << region << mc251;

      if (File::Exists(path))
      {
        if (!CompressFileIntoPacket(path, pac))
          return false;
      }
      else
      {
        // No file, so we'll say the size is 0
        pac << sf::Uint64{0};
      }

      SendChunkedToClients(
          std::move(pac), 1,
          StringFromFormat("Memory Card %c Synchronization", is_slot_a ? 'A' : 'B'));
    }
    else if (SConfig::GetInstance().m_EXIDevice[i] ==
             ExpansionInterface::EXIDEVICE_MEMORYCARDFOLDER)
    {
      const std::string path = File::GetUserPath(D_GCUSER_IDX) + region + DIR_SEP +
                               StringFromFormat("Card %c", is_slot_a ? 'A' : 'B');

      sf::Packet pac;
      pac << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
      pac << static_cast<MessageId>(SYNC_SAVE_DATA_GCI);
      pac << is_slot_a;

      if (File::IsDirectory(path))
      {
        std::vector<std::string> files =
            GCMemcardDirectory::GetFileNamesForGameID(path + DIR_SEP, game->GetGameID());

        pac << static_cast<u8>(files.size());

        for (const std::string& file : files)
        {
          pac << file.substr(file.find_last_of('/') + 1);
          if (!CompressFileIntoPacket(file, pac))
            return false;
        }
      }
      else
      {
        pac << static_cast<u8>(0);
      }

      SendChunkedToClients(
          std::move(pac), 1,
          StringFromFormat("GCI Folder %c Synchronization", is_slot_a ? 'A' : 'B'));
    }
  }

  if (wii_save)
  {
    const auto configured_fs = IOS::HLE::FS::MakeFileSystem(IOS::HLE::FS::Location::Configured);

    std::vector<std::pair<u64, WiiSave::StoragePointer>> saves;
    if (m_settings.m_SyncAllWiiSaves)
    {
      IOS::HLE::Kernel ios;
      for (const u64 title : ios.GetES()->GetInstalledTitles())
      {
        auto save = WiiSave::MakeNandStorage(configured_fs.get(), title);
        saves.push_back(std::make_pair(title, std::move(save)));
      }
    }
    else if (game->GetPlatform() == DiscIO::Platform::WiiDisc ||
             game->GetPlatform() == DiscIO::Platform::WiiWAD)
    {
      auto save = WiiSave::MakeNandStorage(configured_fs.get(), game->GetTitleID());
      saves.push_back(std::make_pair(game->GetTitleID(), std::move(save)));
    }

    std::vector<u64> titles;

    sf::Packet pac;
    pac << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
    pac << static_cast<MessageId>(SYNC_SAVE_DATA_WII);

    // Shove the Mii data into the start the packet
    {
      auto file = configured_fs->OpenFile(IOS::PID_KERNEL, IOS::PID_KERNEL,
                                          Common::GetMiiDatabasePath(), IOS::HLE::FS::Mode::Read);
      if (file)
      {
        pac << true;

        std::vector<u8> file_data(file->GetStatus()->size);
        if (!file->Read(file_data.data(), file_data.size()))
          return false;
        if (!CompressBufferIntoPacket(file_data, pac))
          return false;
      }
      else
      {
        pac << false;  // no mii data
      }
    }

    // Carry on with the save files
    pac << static_cast<u32>(saves.size());

    for (const auto& pair : saves)
    {
      pac << sf::Uint64{pair.first};
      titles.push_back(pair.first);
      const auto& save = pair.second;

      if (save->SaveExists())
      {
        const std::optional<WiiSave::Header> header = save->ReadHeader();
        const std::optional<WiiSave::BkHeader> bk_header = save->ReadBkHeader();
        const std::optional<std::vector<WiiSave::Storage::SaveFile>> files = save->ReadFiles();
        if (!header || !bk_header || !files)
          return false;

        pac << true;  // save exists

        // Header
        pac << sf::Uint64{header->tid};
        pac << header->banner_size << header->permissions << header->unk1;
        for (size_t i = 0; i < header->md5.size(); i++)
          pac << header->md5[i];
        pac << header->unk2;
        for (size_t i = 0; i < header->banner_size; i++)
          pac << header->banner[i];

        // BkHeader
        pac << bk_header->size << bk_header->magic << bk_header->ngid << bk_header->number_of_files
            << bk_header->size_of_files << bk_header->unk1 << bk_header->unk2
            << bk_header->total_size;
        for (size_t i = 0; i < bk_header->unk3.size(); i++)
          pac << bk_header->unk3[i];
        pac << sf::Uint64{bk_header->tid};
        for (size_t i = 0; i < bk_header->mac_address.size(); i++)
          pac << bk_header->mac_address[i];

        // Files
        for (const WiiSave::Storage::SaveFile& file : *files)
        {
          pac << file.mode << file.attributes << static_cast<u8>(file.type) << file.path;

          if (file.type == WiiSave::Storage::SaveFile::Type::File)
          {
            const std::optional<std::vector<u8>>& data = *file.data;
            if (!data || !CompressBufferIntoPacket(*data, pac))
              return false;
          }
        }
      }
      else
      {
        pac << false;  // save does not exist
      }
    }

    // Set titles for host-side loading in WiiRoot
    SetWiiSyncData(nullptr, titles);

    SendChunkedToClients(std::move(pac), 1, "Wii Save Synchronization");
  }

  return true;
}

bool NetPlayServer::SyncCodes()
{
  // Sync Codes is ticked, so set m_codes_synced to false
  m_codes_synced = false;

  // Get Game Path
  const auto game = m_dialog->FindGameFile(m_selected_game);
  if (game == nullptr)
  {
    PanicAlertT("Selected game doesn't exist in game list!");
    return false;
  }

  // Find all INI files
  const auto game_id = game->GetGameID();
  const auto revision = game->GetRevision();
  IniFile globalIni;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(game_id, revision))
    globalIni.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
  IniFile localIni;
  for (const std::string& filename : ConfigLoaders::GetGameIniFilenames(game_id, revision))
    localIni.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);

  // Initialize Number of Synced Players
  m_codes_synced_players = 0;

  // Notify Clients of Incoming Code Sync
  {
    sf::Packet pac;
    pac << static_cast<MessageId>(NP_MSG_SYNC_CODES);
    pac << static_cast<MessageId>(SYNC_CODES_NOTIFY);
    SendAsyncToClients(std::move(pac));
  }
  // Sync Gecko Codes
  {
    // Create a Gecko Code Vector with just the active codes
    std::vector<Gecko::GeckoCode> s_active_codes =
        Gecko::SetAndReturnActiveCodes(Gecko::LoadCodes(globalIni, localIni));

    // Determine Codelist Size
    u16 codelines = 0;
    for (const Gecko::GeckoCode& active_code : s_active_codes)
    {
      NOTICE_LOG(ACTIONREPLAY, "Indexing %s", active_code.name.c_str());
      for (const Gecko::GeckoCode::Code& code : active_code.codes)
      {
        NOTICE_LOG(ACTIONREPLAY, "%08x %08x", code.address, code.data);
        codelines++;
      }
    }

    // Output codelines to send
    NOTICE_LOG(ACTIONREPLAY, "Sending %d Gecko codelines", codelines);

    // Send initial packet. Notify of the sync operation and total number of lines being sent.
    {
      sf::Packet pac;
      pac << static_cast<MessageId>(NP_MSG_SYNC_CODES);
      pac << static_cast<MessageId>(SYNC_CODES_NOTIFY_GECKO);
      pac << codelines;
      SendAsyncToClients(std::move(pac));
    }

    // Send entire codeset in the second packet
    {
      sf::Packet pac;
      pac << static_cast<MessageId>(NP_MSG_SYNC_CODES);
      pac << static_cast<MessageId>(SYNC_CODES_DATA_GECKO);
      // Iterate through the active code vector and send each codeline
      for (const Gecko::GeckoCode& active_code : s_active_codes)
      {
        NOTICE_LOG(ACTIONREPLAY, "Sending %s", active_code.name.c_str());
        for (const Gecko::GeckoCode::Code& code : active_code.codes)
        {
          NOTICE_LOG(ACTIONREPLAY, "%08x %08x", code.address, code.data);
          pac << code.address;
          pac << code.data;
        }
      }
      SendAsyncToClients(std::move(pac));
    }
  }

  // Sync AR Codes
  {
    // Create an AR Code Vector with just the active codes
    std::vector<ActionReplay::ARCode> s_active_codes =
        ActionReplay::ApplyAndReturnCodes(ActionReplay::LoadCodes(globalIni, localIni));

    // Determine Codelist Size
    u16 codelines = 0;
    for (const ActionReplay::ARCode& active_code : s_active_codes)
    {
      NOTICE_LOG(ACTIONREPLAY, "Indexing %s", active_code.name.c_str());
      for (const ActionReplay::AREntry& op : active_code.ops)
      {
        NOTICE_LOG(ACTIONREPLAY, "%08x %08x", op.cmd_addr, op.value);
        codelines++;
      }
    }

    // Output codelines to send
    NOTICE_LOG(ACTIONREPLAY, "Sending %d AR codelines", codelines);

    // Send initial packet. Notify of the sync operation and total number of lines being sent.
    {
      sf::Packet pac;
      pac << static_cast<MessageId>(NP_MSG_SYNC_CODES);
      pac << static_cast<MessageId>(SYNC_CODES_NOTIFY_AR);
      pac << codelines;
      SendAsyncToClients(std::move(pac));
    }

    // Send entire codeset in the second packet
    {
      sf::Packet pac;
      pac << static_cast<MessageId>(NP_MSG_SYNC_CODES);
      pac << static_cast<MessageId>(SYNC_CODES_DATA_AR);
      // Iterate through the active code vector and send each codeline
      for (const ActionReplay::ARCode& active_code : s_active_codes)
      {
        NOTICE_LOG(ACTIONREPLAY, "Sending %s", active_code.name.c_str());
        for (const ActionReplay::AREntry& op : active_code.ops)
        {
          NOTICE_LOG(ACTIONREPLAY, "%08x %08x", op.cmd_addr, op.value);
          pac << op.cmd_addr;
          pac << op.value;
        }
      }
      SendAsyncToClients(std::move(pac));
    }
  }

  return true;
}

void NetPlayServer::CheckSyncAndStartGame()
{
  if (m_saves_synced && m_codes_synced)
  {
    StartGame();
  }
}

bool NetPlayServer::CompressFileIntoPacket(const std::string& file_path, sf::Packet& packet)
{
  File::IOFile file(file_path, "rb");
  if (!file)
  {
    PanicAlertT("Failed to open file \"%s\".", file_path.c_str());
    return false;
  }

  const sf::Uint64 size = file.GetSize();
  packet << size;

  if (size == 0)
    return true;

  std::vector<u8> in_buffer(NETPLAY_LZO_IN_LEN);
  std::vector<u8> out_buffer(NETPLAY_LZO_OUT_LEN);
  std::vector<u8> wrkmem(LZO1X_1_MEM_COMPRESS);

  lzo_uint i = 0;
  while (true)
  {
    lzo_uint32 cur_len = 0;  // number of bytes to read
    lzo_uint out_len = 0;    // number of bytes to write

    if ((i + NETPLAY_LZO_IN_LEN) >= size)
    {
      cur_len = static_cast<lzo_uint32>(size - i);
    }
    else
    {
      cur_len = NETPLAY_LZO_IN_LEN;
    }

    if (cur_len <= 0)
      break;  // EOF

    if (!file.ReadBytes(in_buffer.data(), cur_len))
    {
      PanicAlertT("Error reading file: %s", file_path.c_str());
      return false;
    }

    if (lzo1x_1_compress(in_buffer.data(), cur_len, out_buffer.data(), &out_len, wrkmem.data()) !=
        LZO_E_OK)
    {
      PanicAlertT("Internal LZO Error - compression failed");
      return false;
    }

    // The size of the data to write is 'out_len'
    packet << static_cast<u32>(out_len);
    for (size_t j = 0; j < out_len; j++)
    {
      packet << out_buffer[j];
    }

    if (cur_len != NETPLAY_LZO_IN_LEN)
      break;

    i += cur_len;
  }

  // Mark end of data
  packet << static_cast<u32>(0);

  return true;
}

bool NetPlayServer::CompressBufferIntoPacket(const std::vector<u8>& in_buffer, sf::Packet& packet)
{
  const sf::Uint64 size = in_buffer.size();
  packet << size;

  if (size == 0)
    return true;

  std::vector<u8> out_buffer(NETPLAY_LZO_OUT_LEN);
  std::vector<u8> wrkmem(LZO1X_1_MEM_COMPRESS);

  lzo_uint i = 0;
  while (true)
  {
    lzo_uint32 cur_len = 0;  // number of bytes to read
    lzo_uint out_len = 0;    // number of bytes to write

    if ((i + NETPLAY_LZO_IN_LEN) >= size)
    {
      cur_len = static_cast<lzo_uint32>(size - i);
    }
    else
    {
      cur_len = NETPLAY_LZO_IN_LEN;
    }

    if (cur_len <= 0)
      break;  // end of buffer

    if (lzo1x_1_compress(&in_buffer[i], cur_len, out_buffer.data(), &out_len, wrkmem.data()) !=
        LZO_E_OK)
    {
      PanicAlertT("Internal LZO Error - compression failed");
      return false;
    }

    // The size of the data to write is 'out_len'
    packet << static_cast<u32>(out_len);
    for (size_t j = 0; j < out_len; j++)
    {
      packet << out_buffer[j];
    }

    if (cur_len != NETPLAY_LZO_IN_LEN)
      break;

    i += cur_len;
  }

  // Mark end of data
  packet << static_cast<u32>(0);

  return true;
}

u64 NetPlayServer::GetInitialNetPlayRTC() const
{
  const auto& config = SConfig::GetInstance();

  if (config.bEnableCustomRTC)
    return config.m_customRTCValue;

  return Common::Timer::GetLocalTimeSinceJan1970();
}

// called from multiple threads
void NetPlayServer::SendToClients(const sf::Packet& packet, const PlayerId skip_pid,
                                  const u8 channel_id)
{
  for (auto& p : m_players)
  {
    if (p.second.pid && p.second.pid != skip_pid)
    {
      Send(p.second.socket, packet, channel_id);
    }
  }
}

void NetPlayServer::Send(ENetPeer* socket, const sf::Packet& packet, const u8 channel_id)
{
  ENetPacket* epac =
      enet_packet_create(packet.getData(), packet.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(socket, channel_id, epac);
}

void NetPlayServer::KickPlayer(PlayerId player)
{
  for (auto& current_player : m_players)
  {
    if (current_player.second.pid == player)
    {
      enet_peer_disconnect(current_player.second.socket, 0);
      return;
    }
  }
}

bool NetPlayServer::PlayerHasControllerMapped(const PlayerId pid) const
{
  const auto mapping_matches_player_id = [pid](const PlayerId& mapping) { return mapping == pid; };

  return std::any_of(m_pad_map.begin(), m_pad_map.end(), mapping_matches_player_id) ||
         std::any_of(m_wiimote_map.begin(), m_wiimote_map.end(), mapping_matches_player_id);
}

u16 NetPlayServer::GetPort() const
{
  return m_server->address.port;
}

void NetPlayServer::SetNetPlayUI(NetPlayUI* dialog)
{
  m_dialog = dialog;
}

// called from ---GUI--- thread
std::unordered_set<std::string> NetPlayServer::GetInterfaceSet() const
{
  std::unordered_set<std::string> result;
  auto lst = GetInterfaceListInternal();
  for (auto list_entry : lst)
    result.emplace(list_entry.first);
  return result;
}

// called from ---GUI--- thread
std::string NetPlayServer::GetInterfaceHost(const std::string& inter) const
{
  char buf[16];
  sprintf(buf, ":%d", GetPort());
  auto lst = GetInterfaceListInternal();
  for (const auto& list_entry : lst)
  {
    if (list_entry.first == inter)
    {
      return list_entry.second + buf;
    }
  }
  return "?";
}

// called from ---GUI--- thread
std::vector<std::pair<std::string, std::string>> NetPlayServer::GetInterfaceListInternal() const
{
  std::vector<std::pair<std::string, std::string>> result;
#if defined(_WIN32)

#elif defined(ANDROID)
// Android has no getifaddrs for some stupid reason.  If this
// functionality ends up actually being used on Android, fix this.
#else
  ifaddrs* ifp = nullptr;
  char buf[512];
  if (getifaddrs(&ifp) != -1)
  {
    for (ifaddrs* curifp = ifp; curifp; curifp = curifp->ifa_next)
    {
      sockaddr* sa = curifp->ifa_addr;

      if (sa == nullptr)
        continue;
      if (sa->sa_family != AF_INET)
        continue;
      sockaddr_in* sai = (struct sockaddr_in*)sa;
      if (ntohl(((struct sockaddr_in*)sa)->sin_addr.s_addr) == 0x7f000001)
        continue;
      const char* ip = inet_ntop(sa->sa_family, &sai->sin_addr, buf, sizeof(buf));
      if (ip == nullptr)
        continue;
      result.emplace_back(std::make_pair(curifp->ifa_name, ip));
    }
    freeifaddrs(ifp);
  }
#endif
  if (result.empty())
    result.emplace_back(std::make_pair("!local!", "127.0.0.1"));
  return result;
}

// called from ---Chunked Data--- thread
void NetPlayServer::ChunkedDataThreadFunc()
{
  while (m_do_loop)
  {
    m_chunked_data_event.Wait();

    if (m_abort_chunked_data)
    {
      // thread-safe clear
      while (!m_chunked_data_queue.Empty())
        m_chunked_data_queue.Pop();

      m_abort_chunked_data = false;
    }

    while (!m_chunked_data_queue.Empty())
    {
      if (!m_do_loop)
        return;
      if (m_abort_chunked_data)
        break;
      auto& e = m_chunked_data_queue.Front();
      const u32 id = m_next_chunked_data_id++;

      m_chunked_data_complete_count[id] = 0;
      size_t player_count;
      {
        std::vector<int> players;
        if (e.target_mode == TargetMode::Only)
        {
          players.push_back(e.target_pid);
        }
        else
        {
          for (auto& pl : m_players)
          {
            if (pl.second.pid != e.target_pid)
              players.push_back(pl.second.pid);
          }
        }
        player_count = players.size();

        sf::Packet pac;
        pac << static_cast<MessageId>(NP_MSG_CHUNKED_DATA_START);
        pac << id << e.title << sf::Uint64{e.packet.getDataSize()};

        ChunkedDataSend(std::move(pac), e.target_pid, e.target_mode);

        if (e.target_mode == TargetMode::AllExcept && e.target_pid == 1)
          m_dialog->ShowChunkedProgressDialog(e.title, e.packet.getDataSize(), players);
      }

      const bool enable_limit = Config::Get(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT);
      const float bytes_per_second =
          (std::max(Config::Get(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT), 1u) / 8.0f) * 1024.0f;
      const std::chrono::duration<double> send_interval(CHUNKED_DATA_UNIT_SIZE / bytes_per_second);
      bool skip_wait = false;
      size_t index = 0;
      do
      {
        if (!m_do_loop)
          return;
        if (m_abort_chunked_data)
        {
          sf::Packet pac;
          pac << static_cast<MessageId>(NP_MSG_CHUNKED_DATA_ABORT);
          pac << id;
          ChunkedDataSend(std::move(pac), e.target_pid, e.target_mode);
          break;
        }
        if (e.target_mode == TargetMode::Only)
        {
          if (m_players.find(e.target_pid) == m_players.end())
          {
            skip_wait = true;
            break;
          }
        }

        auto start = std::chrono::steady_clock::now();

        sf::Packet pac;
        pac << static_cast<MessageId>(NP_MSG_CHUNKED_DATA_PAYLOAD);
        pac << id;
        size_t len = std::min(CHUNKED_DATA_UNIT_SIZE, e.packet.getDataSize() - index);
        pac.append(static_cast<const u8*>(e.packet.getData()) + index, len);

        ChunkedDataSend(std::move(pac), e.target_pid, e.target_mode);
        index += CHUNKED_DATA_UNIT_SIZE;

        if (enable_limit)
        {
          std::chrono::duration<double> delta = std::chrono::steady_clock::now() - start;
          std::this_thread::sleep_for(send_interval - delta);
        }
      } while (index < e.packet.getDataSize());

      if (!m_abort_chunked_data)
      {
        sf::Packet pac;
        pac << static_cast<MessageId>(NP_MSG_CHUNKED_DATA_END);
        pac << id;
        ChunkedDataSend(std::move(pac), e.target_pid, e.target_mode);
      }

      while (m_chunked_data_complete_count[id] < player_count && m_do_loop &&
             !m_abort_chunked_data && !skip_wait)
        m_chunked_data_complete_event.Wait();
      m_chunked_data_complete_count.erase(id);
      m_dialog->HideChunkedProgressDialog();

      m_chunked_data_queue.Pop();
    }
  }
}

// called from ---Chunked Data--- thread
void NetPlayServer::ChunkedDataSend(sf::Packet&& packet, const PlayerId pid,
                                    const TargetMode target_mode)
{
  if (target_mode == TargetMode::Only)
  {
    SendAsync(std::move(packet), pid, CHUNKED_DATA_CHANNEL);
  }
  else
  {
    SendAsyncToClients(std::move(packet), pid, CHUNKED_DATA_CHANNEL);
  }
}

void NetPlayServer::ChunkedDataAbort()
{
  m_abort_chunked_data = true;
  m_chunked_data_event.Set();
  m_chunked_data_complete_event.Set();
}
}  // namespace NetPlay
