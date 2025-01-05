// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Slippi/SlippiNetplay.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/ENet.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Timer.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/NetPlayProto.h"
#include "SlippiGame.h"
#include "SlippiPremadeText.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <thread>

static std::mutex pad_mutex;
static std::mutex ack_mutex;

SlippiNetplayClient* SLIPPI_NETPLAY = nullptr;

// called from ---GUI--- thread
SlippiNetplayClient::~SlippiNetplayClient()
{
  m_do_loop.Clear();
  if (m_thread.joinable())
    m_thread.join();

  if (!m_server.empty())
  {
    Disconnect();
  }

  if (Common::g_MainNetHost.get() == m_client)
  {
    Common::g_MainNetHost.release();
  }
  if (m_client)
  {
    enet_host_destroy(m_client);
    m_client = nullptr;
  }

  SLIPPI_NETPLAY = nullptr;

  WARN_LOG_FMT(SLIPPI_ONLINE, "Netplay client cleanup complete");
}

// called from ---SLIPPI EXI--- thread
SlippiNetplayClient::SlippiNetplayClient(std::vector<std::string> addrs, std::vector<u16> ports,
                                         const u8 remote_player_count, const u16 local_port,
                                         bool is_decider, u8 player_idx)
#ifdef _WIN32
    : m_qos_handle(nullptr), m_qos_flow_id(0)
#endif
{
  WARN_LOG_FMT(SLIPPI_ONLINE, "Initializing Slippi Netplay for port: {}, with host: {}", local_port,
               is_decider ? "true" : "false");

  this->is_decider = is_decider;
  this->m_remote_player_count = remote_player_count;
  this->m_player_idx = player_idx;

  // Set up remote player data structures.
  int j = 0;
  for (int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++, j++)
  {
    if (j == m_player_idx)
      j++;
    this->match_info.remote_player_selections[i] = SlippiPlayerSelections();
    this->match_info.remote_player_selections[i].player_idx = j;

    this->m_remote_pad_queue[i] = std::deque<std::unique_ptr<SlippiPad>>();
    this->frame_offset_data[i] = FrameOffsetData();
    this->last_frame_timing[i] = FrameTiming();
    this->ping_us[i] = 0;
    this->last_frame_acked[i] = 0;
  }

  SLIPPI_NETPLAY = std::move(this);

  // Local address
  ENetAddress* local_addr = nullptr;
  ENetAddress local_addr_def;

  // It is important to be able to set the local port to listen on even in a client connection
  // because not doing so will break hole punching, the host is expecting traffic to come from a
  // specific ip/port and if the port does not match what it is expecting, it will not get through
  // the NAT on some routers
  if (local_port > 0)
  {
    INFO_LOG_FMT(SLIPPI_ONLINE, "Setting up local address");

    local_addr_def.host = ENET_HOST_ANY;
    local_addr_def.port = local_port;

    local_addr = &local_addr_def;
  }

  // TODO: Figure out how to use a local port when not hosting without accepting incoming
  // connections
  m_client = enet_host_create(local_addr, 10, 3, 0, 0);

  if (m_client == nullptr)
  {
    PanicAlertFmtT("Couldn't Create Client");
  }

  for (int i = 0; i < remote_player_count; i++)
  {
    ENetAddress addr;
    enet_address_set_host(&addr, addrs[i].c_str());
    addr.port = ports[i];
    // INFO_LOG_FMT(SLIPPI_ONLINE, "Set ENet host, addr = {}, port = {}", addr.host, addr.port);

    ENetPeer* peer = enet_host_connect(m_client, &addr, 3, 0);
    m_server.push_back(peer);

    // Store this connection
    std::stringstream key_strm;
    key_strm << addr.host << "-" << addr.port;
    m_active_connections[key_strm.str()][peer] = true;
    INFO_LOG_FMT(SLIPPI_ONLINE, "New connection (constr): {}", key_strm.str());

    if (peer == nullptr)
    {
      PanicAlertFmtT("Couldn't create peer.");
    }
    else
    {
      /*INFO_LOG_FMT(SLIPPI_ONLINE, "Connecting to ENet host, addr = {}, port = {}",
                   peer->address.host, peer->address.port);*/
    }
  }

  slippi_connect_status = SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED;

  m_thread = std::thread(&SlippiNetplayClient::ThreadFunc, this);
}

// Make a dummy client
SlippiNetplayClient::SlippiNetplayClient(bool is_decider)
{
  this->is_decider = is_decider;
  SLIPPI_NETPLAY = std::move(this);
  slippi_connect_status = SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
}

u8 SlippiNetplayClient::PlayerIdxFromPort(u8 port)
{
  u8 p = port;
  if (port > m_player_idx)
  {
    p--;
  }
  return p;
}

u8 SlippiNetplayClient::LocalPlayerPort()
{
  return this->m_player_idx;
}

// called from ---NETPLAY--- thread
unsigned int SlippiNetplayClient::OnData(sf::Packet& packet, ENetPeer* peer)
{
  u8 message_value = 0;
  if (!(packet >> message_value))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received empty netplay packet");
    return 0;
  }

  NetPlay::MessageID mid{message_value};
  switch (mid)
  {
  case NetPlay::MessageID::SLIPPI_PAD:
  {
    // Fetch current time immediately for the most accurate timing calculations
    u64 curr_time = Common::Timer::NowUs();

    s32 frame;
    s32 checksum_frame;
    u32 checksum;

    if (!(packet >> frame))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay packet too small to read frame count");
      break;
    }

    u8 packet_player_port;
    if (!(packet >> packet_player_port))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay packet too small to read player index");
      break;
    }
    if (!(packet >> checksum_frame))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay packet too small to read checksum frame");
      break;
    }
    if (!(packet >> checksum))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay packet too small to read checksum value");
      break;
    }
    u8 p_idx = PlayerIdxFromPort(packet_player_port);
    if (p_idx >= m_remote_player_count)
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Got packet with invalid player idx {}", p_idx);
      break;
    }

    // This is the amount of bytes from the start of the packet where the pad data starts
    int pad_data_offset = 14;

    // This fetches the m_server index that stores the connection we want to overwrite (if
    // necessary). Note that this index is not necessarily the same as the p_idx because if we have
    // users connecting with the same WAN, the m_server indices might not match
    int conn_idx = 0;
    for (int i = 0; i < m_server.size(); i++)
    {
      if (peer->address.host == m_server[i]->address.host &&
          peer->address.port == m_server[i]->address.port)
      {
        conn_idx = i;
        break;
      }
    }

    // Here we check if we have more than 1 connection for a specific player, this can happen
    // because both players try to connect to each other at the same time to increase the odds that
    // one direction might work and for hole punching. That said there's no point in keeping more
    // than 1 connection alive. I think they might use bandwidth with keep alives or something. Only
    // the lower port player will initiate the disconnect
    std::stringstream key_strm;
    key_strm << peer->address.host << "-" << peer->address.port;
    if (m_active_connections[key_strm.str()].size() > 1 && m_player_idx <= p_idx)
    {
      m_server[conn_idx] = peer;
      INFO_LOG_FMT(
          SLIPPI_ONLINE,
          "Multiple connections detected for single peer. {}:{}. Disconnecting superfluous "
          "connections. oppIdx: {}. p_idx: {}",
          peer->address.host, peer->address.port, p_idx, m_player_idx);

      for (auto active_conn : m_active_connections[key_strm.str()])
      {
        if (active_conn.first == peer)
          continue;

        // Tell our peer to terminate this connection
        enet_peer_disconnect(active_conn.first, 0);
      }
    }

    // Pad received, try to guess what our local time was when the frame was sent by our opponent
    // before we initialized
    // We can compare this to when we sent a pad for last frame to figure out how far/behind we
    // are with respect to the opponent

    auto timing = last_frame_timing[p_idx];
    if (!has_game_started)
    {
      // Handle case where opponent starts sending inputs before our game has reached frame 1. This
      // will continuously say frame 0 is now to prevent opp from getting too far ahead
      timing.frame = 0;
      timing.time_us = curr_time;
    }

    s64 opponent_send_time_us = curr_time - (ping_us[p_idx] / 2);
    s64 frame_diff_offset_us = 16683 * (timing.frame - frame);
    s64 time_offset_us = opponent_send_time_us - timing.time_us + frame_diff_offset_us;

    // INFO_LOG_FMT(SLIPPI_ONLINE, "[Offset] Opp Frame: {}, My Frame: {}. Time offset: {}", frame,
    //              timing.frame, time_offset_us);

    // Add this offset to circular buffer for use later
    if (frame_offset_data[p_idx].buf.size() < SLIPPI_ONLINE_LOCKSTEP_INTERVAL)
      frame_offset_data[p_idx].buf.push_back(static_cast<s32>(time_offset_us));
    else
      frame_offset_data[p_idx].buf[frame_offset_data[p_idx].idx] = (s32)time_offset_us;

    frame_offset_data[p_idx].idx =
        (frame_offset_data[p_idx].idx + 1) % SLIPPI_ONLINE_LOCKSTEP_INTERVAL;

    s64 inputs_to_copy;
    {
      std::lock_guard<std::mutex> lk(pad_mutex);  // TODO: Is this the correct lock?

      auto packet_data = (u8*)packet.getData();

      // INFO_LOG_FMT(SLIPPI_ONLINE, "Receiving a packet of inputs [{}]...", frame);

      // INFO_LOG_FMT(SLIPPI_ONLINE, "Receiving a packet of inputs from player {}({}) [{}]...",
      //              packet_player_port, p_idx, frame);

      s64 frame64 = static_cast<s64>(frame);
      s32 head_frame =
          m_remote_pad_queue[p_idx].empty() ? 0 : m_remote_pad_queue[p_idx].front()->frame;
      inputs_to_copy = frame64 - static_cast<s64>(head_frame);

      // Check that the packet actually contains the data it claims to
      if ((pad_data_offset + inputs_to_copy * SLIPPI_PAD_DATA_SIZE) >
          static_cast<s64>(packet.getDataSize()))
      {
        ERROR_LOG_FMT(
            SLIPPI_ONLINE,
            "Netplay packet too small to read pad buffer. Size: {}, Inputs: {}, MinSize: {}",
            static_cast<int>(packet.getDataSize()), inputs_to_copy,
            pad_data_offset + inputs_to_copy * SLIPPI_PAD_DATA_SIZE);
        break;
      }

      // Not sure what the max is here. If we never ack frames it could get big...
      if (inputs_to_copy > 128)
      {
        ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay packet contained too many frames: {}",
                      inputs_to_copy);
        break;
      }

      for (s64 i = inputs_to_copy - 1; i >= 0; i--)
      {
        auto pad =
            std::make_unique<SlippiPad>(static_cast<s32>(frame64 - i),
                                        &packet_data[pad_data_offset + i * SLIPPI_PAD_DATA_SIZE]);
        m_remote_pad_queue[p_idx].push_front(std::move(pad));
      }

      // Write checksum pad to keep track of latest remote checksum
      ChecksumEntry e;
      e.frame = checksum_frame;
      e.value = checksum;
      remote_checksums[p_idx] = e;
    }

    // Only ack if inputsToCopy is greater than 0. Otherwise we are receiving an old input and
    // we should have already acked something in the future. This can also happen in the case
    // where a new game starts quickly before the remote queue is reset and if we ack the early
    // inputs we will never receive them
    if (inputs_to_copy > 0)
    {
      // Send Ack
      sf::Packet spac;
      spac << static_cast<u8>(NetPlay::MessageID::SLIPPI_PAD_ACK);
      spac << frame;
      spac << m_player_idx;
      // INFO_LOG(SLIPPI_ONLINE, "Sending ack packet for frame %d (player %d) to peer at %d:%d",
      // frame, packet_player_port,
      //         peer->address.host, peer->address.port);

      ENetPacket* epac =
          enet_packet_create(spac.getData(), spac.getDataSize(), ENET_PACKET_FLAG_UNSEQUENCED);
      enet_peer_send(peer, 2, epac);
    }
  }
  break;

  case NetPlay::MessageID::SLIPPI_PAD_ACK:
  {
    std::lock_guard<std::mutex> lk(ack_mutex);  // Trying to fix rare crash on ack_timers.count

    // Store last frame acked
    int32_t frame;
    if (!(packet >> frame))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Ack packet too small to read frame");
      break;
    }

    u8 packet_player_port;
    if (!(packet >> packet_player_port))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay ack packet too small to read player index");
      break;
    }
    u8 p_idx = PlayerIdxFromPort(packet_player_port);
    if (p_idx >= m_remote_player_count)
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Got ack packet with invalid player idx {}", p_idx);
      break;
    }

    // INFO_LOG_FMT(SLIPPI_ONLINE, "Received ack packet from player {}({}) [{}]...",
    // packet_player_port,
    //              p_idx, frame);

    last_frame_acked[p_idx] = frame > last_frame_acked[p_idx] ? frame : last_frame_acked[p_idx];

    // Remove old timings
    while (!ack_timers[p_idx].Empty() && ack_timers[p_idx].Front().frame < frame)
    {
      ack_timers[p_idx].Pop();
    }

    // Don't get a ping if we do not have the right ack frame
    if (ack_timers[p_idx].Empty() || ack_timers[p_idx].Front().frame != frame)
    {
      break;
    }

    auto send_time = ack_timers[p_idx].Front().time_us;
    ack_timers[p_idx].Pop();

    ping_us[p_idx] = Common::Timer::NowUs() - send_time;
    if (g_ActiveConfig.bShowNetPlayPing && frame % SLIPPI_PING_DISPLAY_INTERVAL == 0 && p_idx == 0)
    {
      std::stringstream ping_display;
      ping_display << "Ping: " << (ping_us[0] / 1000);
      for (int i = 1; i < m_remote_player_count; i++)
      {
        ping_display << " | " << (ping_us[i] / 1000);
      }
      OSD::AddTypedMessage(OSD::MessageType::NetPlayPing, ping_display.str(), OSD::Duration::NORMAL,
                           OSD::Color::CYAN);
    }
  }
  break;

  case NetPlay::MessageID::SLIPPI_MATCH_SELECTIONS:
  {
    auto s = readSelectionsFromPacket(packet);
    if (!s->error)
    {
      INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Received selections from opponent with player idx {}",
                   s->player_idx);
      u8 idx = PlayerIdxFromPort(s->player_idx);
      if (idx >= m_remote_player_count)
      {
        ERROR_LOG_FMT(SLIPPI_ONLINE, "Got match selection packet with invalid player idx {}", idx);
        break;
      }
      match_info.remote_player_selections[idx].Merge(*s);

      // This might be a good place to reset some logic? Game can't start until we receive this msg
      // so this should ensure that everything is initialized before the game starts
      has_game_started = false;

      // Reset remote pad queue such that next inputs that we get are not compared to inputs from
      // last game
      m_remote_pad_queue[idx].clear();
    }
  }
  break;

  case NetPlay::MessageID::SLIPPI_CHAT_MESSAGE:
  {
    auto player_selection = ReadChatMessageFromPacket(packet);
    INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Received chat message from opponent {}: {}",
                 player_selection->player_idx, player_selection->message_id);

    if (!player_selection->error)
    {
      // set message id to netplay instance
      remote_chat_message_selection = std::move(player_selection);
    }
  }
  break;

  case NetPlay::MessageID::SLIPPI_CONN_SELECTED:
  {
    // Currently this is unused but the intent is to support two-way simultaneous connection
    // attempts
    is_connection_selected = true;
  }
  break;

  case NetPlay::MessageID::SLIPPI_COMPLETE_STEP:
  {
    SlippiGamePrepStepResults results;

    packet >> results.step_idx;
    packet >> results.char_selection;
    packet >> results.char_color_selection;
    packet >> results.stage_selections[0];
    packet >> results.stage_selections[1];

    game_prep_step_queue.push_back(results);
  }
  break;

  case NetPlay::MessageID::SLIPPI_SYNCED_STATE:
  {
    u8 packet_player_port;
    if (!(packet >> packet_player_port))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay packet too small to read player index");
      break;
    }
    u8 p_idx = PlayerIdxFromPort(packet_player_port);
    if (p_idx >= m_remote_player_count)
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Got packet with invalid player idx {}", p_idx);
      break;
    }

    SlippiSyncedGameState results;
    packet >> results.match_id;
    packet >> results.game_idx;
    packet >> results.tiebreak_idx;
    packet >> results.seconds_remaining;
    for (int i = 0; i < 4; i++)
    {
      packet >> results.fighters[i].stocks_remaining;
      packet >> results.fighters[i].current_health;
    }

    // ERROR_LOG_FMT(SLIPPI_ONLINE, "Received synced state from opponent. {}, {}, {}, {}. F1: {}
    // ({}%%), F2: {} ({}%%)",
    //          results.match_id, results.game_idx, results.tiebreak_idx,
    //          results.seconds_remaining, results.fighters[0].stocks_remaining,
    //          results.fighters[0].current_health, results.fighters[1].stocks_remaining,
    //          results.fighters[1].current_health);

    remote_sync_states[p_idx] = results;
  }
  break;

  default:
    WARN_LOG_FMT(SLIPPI_ONLINE, "Unknown message received with id : {}", static_cast<u8>(mid));
    break;
  }

  return 0;
}

void SlippiNetplayClient::writeToPacket(sf::Packet& packet, SlippiPlayerSelections& s)
{
  packet << static_cast<u8>(NetPlay::MessageID::SLIPPI_MATCH_SELECTIONS);
  packet << s.character_id << s.character_color << s.is_character_selected;
  packet << s.player_idx;
  packet << s.stage_id << s.is_stage_selected;
  packet << s.rng_offset;
  packet << s.team_id;
}

void SlippiNetplayClient::WriteChatMessageToPacket(sf::Packet& packet, int message_id, u8 player_id)
{
  packet << static_cast<u8>(NetPlay::MessageID::SLIPPI_CHAT_MESSAGE);
  packet << message_id;
  packet << player_id;
}

std::unique_ptr<SlippiPlayerSelections>
SlippiNetplayClient::ReadChatMessageFromPacket(sf::Packet& packet)
{
  auto s = std::make_unique<SlippiPlayerSelections>();

  if (!(packet >> s->message_id))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Chat packet too small to read message ID");
    s->error = true;
    return std::move(s);
  }
  if (!(packet >> s->player_idx))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Chat packet too small to read player index");
    s->error = true;
    return std::move(s);
  }

  switch (s->message_id)
  {
  // Only these 16 message IDs are allowed
  case 136:
  case 129:
  case 130:
  case 132:
  case 34:
  case 40:
  case 33:
  case 36:
  case 72:
  case 66:
  case 68:
  case 65:
  case 24:
  case 18:
  case 20:
  case 17:
  case SlippiPremadeText::CHAT_MSG_CHAT_DISABLED:  // Opponent Chat Message Disabled
  {
    // Good message ID. Do nothing
    break;
  }
  default:
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid chat message index: {}", s->message_id);
    s->error = true;
    break;
  }
  }

  return std::move(s);
}

std::unique_ptr<SlippiPlayerSelections>
SlippiNetplayClient::readSelectionsFromPacket(sf::Packet& packet)
{
  auto s = std::make_unique<SlippiPlayerSelections>();

  if (!(packet >> s->character_id))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->character_color))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->is_character_selected))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->player_idx))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->stage_id))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->is_stage_selected))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->rng_offset))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->team_id))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }

  return std::move(s);
}

void SlippiNetplayClient::Send(sf::Packet& packet)
{
  enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
  u8 channel_id = 0;

  for (int i = 0; i < m_server.size(); i++)
  {
    NetPlay::MessageID mid{((u8*)packet.getData())[0]};
    if (mid == NetPlay::MessageID::SLIPPI_PAD || mid == NetPlay::MessageID::SLIPPI_PAD_ACK)
    {
      // Slippi communications do not need reliable connection and do not need to
      // be received in order. Channel is changed so that other reliable communications
      // do not block anything. This may not be necessary if order is not maintained?
      flags = ENET_PACKET_FLAG_UNSEQUENCED;
      channel_id = 1;
    }

    ENetPacket* epac = enet_packet_create(packet.getData(), packet.getDataSize(), flags);
    enet_peer_send(m_server[i], channel_id, epac);
  }
}

void SlippiNetplayClient::Disconnect()
{
  ENetEvent net_event;
  slippi_connect_status = SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
  if (m_active_connections.empty())
    return;

  for (auto conn : m_active_connections)
  {
    for (auto peer : conn.second)
    {
      INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Disconnecting peer {}", peer.first->address.port);
      enet_peer_disconnect(peer.first, 0);
    }
  }

  while (enet_host_service(m_client, &net_event, 3000) > 0)
  {
    switch (net_event.type)
    {
    case ENET_EVENT_TYPE_RECEIVE:
      enet_packet_destroy(net_event.packet);
      break;
    case ENET_EVENT_TYPE_DISCONNECT:
      INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Got disconnect from peer {}",
                   net_event.peer->address.port);
      break;
    default:
      break;
    }
  }
  // didn't disconnect gracefully force disconnect
  for (auto conn : m_active_connections)
  {
    for (auto peer : conn.second)
    {
      enet_peer_reset(peer.first);
    }
  }
  m_active_connections.clear();
  m_server.clear();
  SLIPPI_NETPLAY = nullptr;
}

void SlippiNetplayClient::SendAsync(std::unique_ptr<sf::Packet> packet)
{
  {
    std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
    m_async_queue.Push(std::move(packet));
  }
  Common::ENet::WakeupThread(m_client);
}

// called from ---NETPLAY--- thread
void SlippiNetplayClient::ThreadFunc()
{
  // Let client die 1 second before host such that after a swap, the client won't be connected to
  u64 start_time = Common::Timer::NowMs();
  u64 timeout = 8000;

  std::vector<bool> connections;
  std::vector<ENetAddress> remote_addrs;
  for (int i = 0; i < m_remote_player_count; i++)
  {
    remote_addrs.push_back(m_server[i]->address);
    connections.push_back(false);
  }

  while (slippi_connect_status == SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED)
  {
    // This will confirm that connection went through successfully
    ENetEvent net_event;
    int net = enet_host_service(m_client, &net_event, 500);
    if (net > 0)
    {
      sf::Packet rpac;
      switch (net_event.type)
      {
      case ENET_EVENT_TYPE_RECEIVE:
        if (!net_event.peer)
        {
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got receive event with nil peer");
          continue;
        }
        INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got receive event with peer addr {}:{}",
                     net_event.peer->address.host, net_event.peer->address.port);
        rpac.append(net_event.packet->data, net_event.packet->dataLength);

        OnData(rpac, net_event.peer);

        enet_packet_destroy(net_event.packet);
        break;

      case ENET_EVENT_TYPE_DISCONNECT:
        if (!net_event.peer)
        {
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got disconnect event with nil peer");
          continue;
        }
        INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got disconnect event with peer addr {}:{}",
                     net_event.peer->address.host, net_event.peer->address.port);
        break;

      case ENET_EVENT_TYPE_CONNECT:
      {
        if (!net_event.peer)
        {
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got connect event with nil peer");
          continue;
        }

        std::stringstream key_strm;
        key_strm << net_event.peer->address.host << "-" << net_event.peer->address.port;
        m_active_connections[key_strm.str()][net_event.peer] = true;
        INFO_LOG_FMT(SLIPPI_ONLINE, "New connection (early): {}", key_strm.str().c_str());

        INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got connect event with peer addr {}:{}",
                     net_event.peer->address.host, net_event.peer->address.port);

        auto is_already_connected = false;
        for (int i = 0; i < m_server.size(); i++)
        {
          if (connections[i] && net_event.peer->address.host == m_server[i]->address.host &&
              net_event.peer->address.port == m_server[i]->address.port)
          {
            m_server[i] = net_event.peer;
            is_already_connected = true;
            break;
          }
        }

        if (is_already_connected)
        {
          // Don't add this person again if they are already connected. Not doing this can cause
          // one person to take up 2 or more spots, denying one or more players from connecting
          // and thus getting stuck on the "Waiting" step
          INFO_LOG_FMT(SLIPPI_ONLINE, "Already connected!");
          break;  // Breaks out of case
        }

        for (int i = 0; i < m_server.size(); i++)
        {
          // This check used to check for port as well as host. The problem was that for some
          // people, their internet will switch the port they're sending from. This means these
          // people struggle to connect to others but they sometimes do succeed. When we were
          // checking for port here though we would get into a state where the person they
          // succeeded to connect to would not accept the connection with them, this would lead
          // the player with this internet issue to get stuck waiting for the other player. The
          // only downside to this that I can guess is that if you fail to connect to one person
          // out of two that are on your LAN, it might report that you failed to connect to the
          // wrong person. There might be more problems tho, not sure
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Comparing connection address: {} - {}",
                       static_cast<u32>(remote_addrs[i].host),
                       static_cast<u32>(net_event.peer->address.host));
          if (remote_addrs[i].host == net_event.peer->address.host && !connections[i])
          {
            INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Overwriting ENetPeer for address: {}:{}",
                         static_cast<u32>(net_event.peer->address.host),
                         static_cast<u32>(net_event.peer->address.port));
            INFO_LOG_FMT(SLIPPI_ONLINE,
                         "[Netplay] Overwriting ENetPeer with id ({}) with new peer of id {}",
                         static_cast<u32>(m_server[i]->connectID),
                         static_cast<u32>(net_event.peer->connectID));
            m_server[i] = net_event.peer;
            connections[i] = true;
            break;
          }
        }
        break;
      }
      }
    }

    bool all_connected = true;
    for (int i = 0; i < m_remote_player_count; i++)
    {
      if (!connections[i])
        all_connected = false;
    }

    if (all_connected)
    {
      m_client->intercept = Common::ENet::InterceptCallback;
      INFO_LOG_FMT(SLIPPI_ONLINE, "Slippi online connection successful!");
      slippi_connect_status = SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED;
      break;
    }

    for (int i = 0; i < m_remote_player_count; i++)
    {
      INFO_LOG_FMT(SLIPPI_ONLINE, "m_client peer {} state: {}", i,
                   static_cast<u8>(m_client->peers[i].state));
    }
    INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Not yet connected. Res: {}, Type: {}", net,
                 static_cast<u8>(net_event.type));

    // Time out after enough time has passed
    u64 curr_time = Common::Timer::NowMs();
    if ((curr_time - start_time) >= timeout || !m_do_loop.IsSet())
    {
      for (int i = 0; i < m_remote_player_count; i++)
      {
        if (!connections[i])
        {
          failed_connections.push_back(i);
        }
      }

      slippi_connect_status = SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
      INFO_LOG_FMT(SLIPPI_ONLINE, "Slippi online connection failed");
      return;
    }
  }

  INFO_LOG_FMT(SLIPPI_ONLINE, "Successfully initialized {} connections", m_server.size());
  for (int i = 0; i < m_server.size(); i++)
  {
    INFO_LOG_FMT(SLIPPI_ONLINE, "Connection {}: {}, {}", i,
                 static_cast<u32>(m_server[i]->address.host),
                 static_cast<u16>(m_server[i]->address.port));
  }

  bool qos_success = false;
#ifdef _WIN32
  QOS_VERSION ver = {1, 0};

  if (QOSCreateHandle(&ver, &m_qos_handle))
  {
    for (int i = 0; i < m_server.size(); i++)
    {
      // from win32.c
      struct sockaddr_in sin = {0};

      sin.sin_family = AF_INET;
      sin.sin_port = ENET_HOST_TO_NET_16(m_server[i]->host->address.port);
      sin.sin_addr.s_addr = m_server[i]->host->address.host;

      if (QOSAddSocketToFlow(m_qos_handle, m_server[i]->host->socket,
                             reinterpret_cast<PSOCKADDR>(&sin),
                             // this is 0x38
                             QOSTrafficTypeControl, QOS_NON_ADAPTIVE_FLOW, &m_qos_flow_id))
      {
        DWORD dscp = 0x2e;

        // this will fail if we're not admin
        // sets DSCP to the same as linux (0x2e)
        QOSSetFlow(m_qos_handle, m_qos_flow_id, QOSSetOutgoingDSCPValue, sizeof(DWORD), &dscp, 0,
                   nullptr);

        qos_success = true;
      }
    }
  }
#else
  for (int i = 0; i < m_server.size(); i++)
  {
#ifdef __linux__
    // highest priority
    int priority = 7;
    setsockopt(m_server[i]->host->socket, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
#endif

    // https://www.tucny.com/Home/dscp-tos
    // ef is better than cs7
    int tos_val = 0xb8;
    qos_success =
        setsockopt(m_server[i]->host->socket, IPPROTO_IP, IP_TOS, &tos_val, sizeof(tos_val)) == 0;
  }
#endif

  while (m_do_loop.IsSet())
  {
    ENetEvent net_event;
    int net;
    net = enet_host_service(m_client, &net_event, 250);
    while (!m_async_queue.Empty())
    {
      Send(*(m_async_queue.Front().get()));
      m_async_queue.Pop();
    }
    if (net > 0)
    {
      sf::Packet rpac;
      switch (net_event.type)
      {
      case ENET_EVENT_TYPE_RECEIVE:
      {
        rpac.append(net_event.packet->data, net_event.packet->dataLength);
        OnData(rpac, net_event.peer);
        enet_packet_destroy(net_event.packet);
        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT:
      {
        std::stringstream key_strm;
        key_strm << net_event.peer->address.host << "-" << net_event.peer->address.port;
        m_active_connections[key_strm.str()].erase(net_event.peer);

        // Check to make sure this address+port are one of the ones we are actually connected to.
        // When connecting to someone that randomizes ports, you can get one valid connection from
        // one port and a failed connection on another port. We don't want to cause a real
        // disconnect if we receive a disconnect message from the port we never connected to
        bool is_connected_client = false;
        for (int i = 0; i < m_server.size(); i++)
        {
          if (net_event.peer->address.host == m_server[i]->address.host &&
              net_event.peer->address.port == m_server[i]->address.port)
          {
            is_connected_client = true;
            break;
          }
        }

        INFO_LOG_FMT(
            SLIPPI_ONLINE,
            "[Netplay] Disconnect late {}:{}. Remaining connections: {}. Is connected client: {}",
            net_event.peer->address.host, net_event.peer->address.port,
            m_active_connections[key_strm.str()].size(), is_connected_client ? "true" : "false");

        // If the disconnect event doesn't come from the client we are actually listening to,
        // it can be safely ignored
        if (is_connected_client && m_active_connections[key_strm.str()].empty())
        {
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Final disconnect received for a client.");
          m_do_loop.Clear();  // Stop the loop, will trigger a disconnect
        }
        break;
      }
      case ENET_EVENT_TYPE_CONNECT:
      {
        std::stringstream key_strm;
        key_strm << net_event.peer->address.host << "-" << net_event.peer->address.port;
        m_active_connections[key_strm.str()][net_event.peer] = true;
        INFO_LOG_FMT(SLIPPI_ONLINE, "New connection (late): {}", key_strm.str().c_str());
        break;
      }
      default:
        break;
      }
    }
  }

#ifdef _WIN32
  if (m_qos_handle != 0)
  {
    if (m_qos_flow_id != 0)
    {
      for (int i = 0; i < m_server.size(); i++)
      {
        QOSRemoveSocketFromFlow(m_qos_handle, m_server[i]->host->socket, m_qos_flow_id, 0);
      }
    }
    QOSCloseHandle(m_qos_handle);
  }
#endif

  Disconnect();
  return;
}

bool SlippiNetplayClient::IsDecider()
{
  return is_decider;
}

bool SlippiNetplayClient::IsConnectionSelected()
{
  return is_connection_selected;
}

SlippiNetplayClient::SlippiConnectStatus SlippiNetplayClient::GetSlippiConnectStatus()
{
  return slippi_connect_status;
}

std::vector<int> SlippiNetplayClient::GetFailedConnections()
{
  return failed_connections;
}

void SlippiNetplayClient::StartSlippiGame()
{
  // Reset variables to start a new game
  has_game_started = false;

  m_local_pad_queue.clear();

  for (int i = 0; i < m_remote_player_count; i++)
  {
    FrameTiming timing;
    timing.frame = 0;
    timing.time_us = Common::Timer::NowUs();
    last_frame_timing[i] = timing;
    last_frame_acked[i] = 0;

    // Reset ack timers
    ack_timers[i].Clear();
  }

  is_desync_recovery = false;

  // Clear game prep queue in case anything is still lingering
  game_prep_step_queue.clear();

  // Reset match info for next game
  match_info.Reset();
}

void SlippiNetplayClient::SendConnectionSelected()
{
  is_connection_selected = true;
  auto spac = std::make_unique<sf::Packet>();
  *spac << static_cast<u8>(NetPlay::MessageID::SLIPPI_CONN_SELECTED);
  SendAsync(std::move(spac));
}

void SlippiNetplayClient::SendSlippiPad(std::unique_ptr<SlippiPad> pad)
{
  auto status = slippi_connect_status;
  bool connection_failed =
      status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
  bool connection_disconnected =
      status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
  if (connection_failed || connection_disconnected)
  {
    return;
  }

  if (pad)
  {
    // Add latest local pad report to queue
    m_local_pad_queue.push_front(std::move(pad));
  }

  // Remove pad reports that have been received and acked
  int min_ack_frame = last_frame_acked[0];
  for (int i = 1; i < m_remote_player_count; i++)
  {
    if (last_frame_acked[i] < min_ack_frame)
      min_ack_frame = last_frame_acked[i];
  }
  /*INFO_LOG_FMT(SLIPPI_ONLINE,
               "Checking to drop local inputs, oldest frame: {} | min_ack_frame: {} | {}, {}, {}",
               m_local_pad_queue.back()->frame, min_ack_frame, last_frame_acked[0],
     last_frame_acked[1], last_frame_acked[2]);*/
  while (!m_local_pad_queue.empty() && m_local_pad_queue.back()->frame < min_ack_frame)
  {
    /*INFO_LOG_FMT(SLIPPI_ONLINE, "Dropping local input for frame {} from queue",
                 m_local_pad_queue.back()->frame);*/
    m_local_pad_queue.pop_back();
  }

  if (m_local_pad_queue.empty())
  {
    // If pad queue is empty now, there's no reason to send anything
    return;
  }
  auto frame = m_local_pad_queue.front()->frame;
  auto spac = std::make_unique<sf::Packet>();
  *spac << static_cast<u8>(NetPlay::MessageID::SLIPPI_PAD);
  *spac << frame;
  *spac << this->m_player_idx;
  *spac << m_local_pad_queue.front()->checksum_frame;
  *spac << m_local_pad_queue.front()->checksum;

  for (auto it = m_local_pad_queue.begin(); it != m_local_pad_queue.end(); ++it)
    spac->append((*it)->pad_buf, SLIPPI_PAD_DATA_SIZE);  // only transfer 8 bytes per pad

  SendAsync(std::move(spac));
  u64 time = Common::Timer::NowUs();

  has_game_started = true;

  for (int i = 0; i < m_remote_player_count; i++)
  {
    FrameTiming timing;
    timing.frame = frame;
    timing.time_us = time;
    last_frame_timing[i] = timing;

    // Add send time to ack timers
    FrameTiming send_time;
    send_time.frame = frame;
    send_time.time_us = time;
    ack_timers[i].Push(send_time);
  }
}

void SlippiNetplayClient::SetMatchSelections(SlippiPlayerSelections& s)
{
  match_info.local_player_selections.Merge(s);
  match_info.local_player_selections.player_idx = m_player_idx;

  // Send packet containing selections
  auto spac = std::make_unique<sf::Packet>();
  writeToPacket(*spac, match_info.local_player_selections);
  SendAsync(std::move(spac));
}

void SlippiNetplayClient::SendGamePrepStep(SlippiGamePrepStepResults& s)
{
  auto spac = std::make_unique<sf::Packet>();
  *spac << static_cast<u8>(NetPlay::MessageID::SLIPPI_COMPLETE_STEP);
  *spac << s.step_idx;
  *spac << s.char_selection;
  *spac << s.char_color_selection;
  *spac << s.stage_selections[0] << s.stage_selections[1];
  SendAsync(std::move(spac));
}

void SlippiNetplayClient::SendSyncedGameState(SlippiSyncedGameState& s)
{
  is_desync_recovery = true;
  local_sync_state = s;

  auto spac = std::make_unique<sf::Packet>();
  *spac << static_cast<u8>(NetPlay::MessageID::SLIPPI_SYNCED_STATE);
  *spac << this->m_player_idx;
  *spac << s.match_id;
  *spac << s.game_idx;
  *spac << s.tiebreak_idx;
  *spac << s.seconds_remaining;
  for (int i = 0; i < 4; i++)
  {
    *spac << s.fighters[i].stocks_remaining;
    *spac << s.fighters[i].current_health;
  }
  SendAsync(std::move(spac));
}

bool SlippiNetplayClient::GetGamePrepResults(u8 step_idx, SlippiGamePrepStepResults& res)
{
  // Just pull stuff off until we find something for the right step. I think that should be fine
  while (!game_prep_step_queue.empty())
  {
    auto front = game_prep_step_queue.front();
    if (front.step_idx == step_idx)
    {
      res = front;
      return true;
    }

    game_prep_step_queue.pop_front();
  }

  return false;
}

SlippiPlayerSelections SlippiNetplayClient::GetSlippiRemoteChatMessage(bool isChatEnabled)
{
  SlippiPlayerSelections copied_selection = SlippiPlayerSelections();

  if (remote_chat_message_selection != nullptr && isChatEnabled)
  {
    copied_selection.message_id = remote_chat_message_selection->message_id;
    copied_selection.player_idx = remote_chat_message_selection->player_idx;

    // Clear it out
    remote_chat_message_selection->message_id = 0;
    remote_chat_message_selection->player_idx = 0;
  }
  else
  {
    copied_selection.message_id = 0;
    copied_selection.player_idx = 0;

    // if chat is not enabled, automatically send back a message saying so.
    if (remote_chat_message_selection != nullptr && !isChatEnabled &&
        (remote_chat_message_selection->message_id > 0 &&
         remote_chat_message_selection->message_id != SlippiPremadeText::CHAT_MSG_CHAT_DISABLED))
    {
      auto packet = std::make_unique<sf::Packet>();
      remote_sent_chat_message_id = SlippiPremadeText::CHAT_MSG_CHAT_DISABLED;
      WriteChatMessageToPacket(*packet, remote_sent_chat_message_id, LocalPlayerPort());
      SendAsync(std::move(packet));
      remote_sent_chat_message_id = 0;
      remote_chat_message_selection = nullptr;
    }
  }

  return copied_selection;
}

u8 SlippiNetplayClient::GetSlippiRemoteSentChatMessage(bool isChatEnabled)
{
  if (!isChatEnabled)
  {
    return 0;
  }

  u8 copied_message_id = remote_sent_chat_message_id;
  remote_sent_chat_message_id = 0;  // Clear it out
  return copied_message_id;
}

std::unique_ptr<SlippiRemotePadOutput> SlippiNetplayClient::GetFakePadOutput(int frame)
{
  // Used for testing purposes, will ignore the opponent's actual inputs and provide fake
  // ones to trigger rollback scenarios
  std::unique_ptr<SlippiRemotePadOutput> pad_output = std::make_unique<SlippiRemotePadOutput>();

  // Triggers rollback where the first few inputs were correctly predicted
  if (frame % 60 < 5)
  {
    // Return old inputs for a bit
    pad_output->latest_frame = frame - (frame % 60);
    pad_output->data.insert(pad_output->data.begin(), SLIPPI_PAD_FULL_SIZE, 0);
  }
  else if (frame % 60 == 5)
  {
    pad_output->latest_frame = frame;
    // Add 5 frames of 0'd inputs
    pad_output->data.insert(pad_output->data.begin(), 5 * SLIPPI_PAD_FULL_SIZE, 0);

    // Press A button for 2 inputs prior to this frame causing a rollback
    pad_output->data[2 * SLIPPI_PAD_FULL_SIZE] = 1;
  }
  else
  {
    pad_output->latest_frame = frame;
    pad_output->data.insert(pad_output->data.begin(), SLIPPI_PAD_FULL_SIZE, 0);
  }

  return std::move(pad_output);
}

std::unique_ptr<SlippiRemotePadOutput> SlippiNetplayClient::GetSlippiRemotePad(int index,
                                                                               int maxFrameCount)
{
  std::lock_guard<std::mutex> lk(pad_mutex);  // TODO: Is this the correct lock?

  std::unique_ptr<SlippiRemotePadOutput> pad_output = std::make_unique<SlippiRemotePadOutput>();

  if (m_remote_pad_queue[index].empty())
  {
    auto empty_pad = std::make_unique<SlippiPad>(0);

    pad_output->latest_frame = empty_pad->frame;

    auto empty_it = std::begin(empty_pad->pad_buf);
    pad_output->data.insert(pad_output->data.end(), empty_it, empty_it + SLIPPI_PAD_FULL_SIZE);

    return std::move(pad_output);
  }

  int input_count = 0;

  pad_output->latest_frame = 0;
  pad_output->checksum_frame = remote_checksums[index].frame;
  pad_output->checksum = remote_checksums[index].value;

  // Copy inputs from the remote pad queue to the output. We iterate backwards because
  // we want to get the oldest frames possible (will have been cleared to contain the last
  // finalized frame at the back). I think it's very unlikely but I think before we
  // iterated from the front and it's possible the 7 frame limit left out an input the
  // game actually needed.
  for (auto it = m_remote_pad_queue[index].rbegin(); it != m_remote_pad_queue[index].rend(); ++it)
  {
    if ((*it)->frame > pad_output->latest_frame)
      pad_output->latest_frame = (*it)->frame;

    // NOTICE_LOG(SLIPPI_ONLINE, "[%d] (Remote) P%d %08X %08X %08X", (*it)->frame,
    //						index >= player_idx ? index + 1 : index, Common::swap32(&(*it)->padBuf[0]),
    //						Common::swap32(&(*it)->padBuf[4]), Common::swap32(&(*it)->padBuf[8]));

    auto pad_it = std::begin((*it)->pad_buf);
    pad_output->data.insert(pad_output->data.begin(), pad_it, pad_it + SLIPPI_PAD_FULL_SIZE);

    // Limit max amount of inputs to send
    input_count++;
    if (input_count >= maxFrameCount)
      break;
  }

  return std::move(pad_output);
}

void SlippiNetplayClient::DropOldRemoteInputs(int32_t finalizedFrame)
{
  std::lock_guard<std::mutex> lk(pad_mutex);

  for (int i = 0; i < m_remote_player_count; i++)
  {
    while (m_remote_pad_queue[i].size() > 1 && m_remote_pad_queue[i].back()->frame < finalizedFrame)
      m_remote_pad_queue[i].pop_back();
  }
}

SlippiMatchInfo* SlippiNetplayClient::GetMatchInfo()
{
  return &match_info;
}

int32_t SlippiNetplayClient::GetSlippiLatestRemoteFrame(int maxFrameCount)
{
  // Return the lowest frame among remote queues
  int lowest_frame = 0;
  bool is_frame_set = false;
  for (int i = 0; i < m_remote_player_count; i++)
  {
    auto rp = GetSlippiRemotePad(i, maxFrameCount);
    int f = rp->latest_frame;
    if (f < lowest_frame || !is_frame_set)
    {
      lowest_frame = f;
      is_frame_set = true;
    }
  }

  return lowest_frame;
}

// return the smallest time offset among all remote players
s32 SlippiNetplayClient::CalcTimeOffsetUs()
{
  bool empty = true;
  for (int i = 0; i < m_remote_player_count; i++)
  {
    if (!frame_offset_data[i].buf.empty())
    {
      empty = false;
      break;
    }
  }
  if (empty)
  {
    return 0;
  }

  std::vector<int> offsets;
  for (int i = 0; i < m_remote_player_count; i++)
  {
    if (frame_offset_data[i].buf.empty())
      continue;

    std::vector<s32> buf;
    std::copy(frame_offset_data[i].buf.begin(), frame_offset_data[i].buf.end(),
              std::back_inserter(buf));

    // TODO: Does this work?
    std::sort(buf.begin(), buf.end());

    int buf_size = (int)buf.size();
    int offset = (int)((1.0f / 3.0f) * buf_size);
    int end = buf_size - offset;

    int sum = 0;
    for (int j = offset; j < end; j++)
    {
      sum += buf[j];
    }

    int count = end - offset;
    if (count <= 0)
    {
      return 0;  // What do I return here?
    }

    s32 result = sum / count;
    offsets.push_back(result);
  }

  s32 min_offset = offsets.front();
  for (int i = 1; i < offsets.size(); i++)
  {
    if (offsets[i] < min_offset)
      min_offset = offsets[i];
  }

  return min_offset;
}

bool SlippiNetplayClient::IsWaitingForDesyncRecovery()
{
  // If we are not in a desync recovery state, we do not need to wait
  if (!is_desync_recovery)
    return false;

  for (int i = 0; i < m_remote_player_count; i++)
  {
    if (local_sync_state.game_idx != remote_sync_states[i].game_idx)
      return true;

    if (local_sync_state.tiebreak_idx != remote_sync_states[i].tiebreak_idx)
      return true;
  }

  return false;
}

SlippiDesyncRecoveryResp SlippiNetplayClient::GetDesyncRecoveryState()
{
  SlippiDesyncRecoveryResp result;

  result.is_recovering = is_desync_recovery;
  result.is_waiting = IsWaitingForDesyncRecovery();

  // If we are not recovering or if we are currently waiting, don't need to compute state, just
  // return
  if (!result.is_recovering || result.is_waiting)
    return result;

  result.state = local_sync_state;

  // Here let's try to reconcile all the states into one. This is important to make sure
  // everyone starts at the same percent/stocks because their last synced state might be
  // a slightly different frame. There didn't seem to be an easy way to guarantee they'd
  // all be on exactly the same frame
  for (int i = 0; i < m_remote_player_count; i++)
  {
    auto& s = remote_sync_states[i];
    if (abs(static_cast<int>(result.state.seconds_remaining) -
            static_cast<int>(s.seconds_remaining)) > 1)
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Timer values for desync recovery too different: {}, {}",
                    result.state.seconds_remaining, s.seconds_remaining);
      result.is_error = true;
      return result;
    }

    // Use the timer with more time remaining
    if (s.seconds_remaining > result.state.seconds_remaining)
    {
      result.state.seconds_remaining = s.seconds_remaining;
    }

    for (int j = 0; j < 4; j++)
    {
      auto& fighter = result.state.fighters[i];
      auto& i_fighter = s.fighters[i];

      if (fighter.stocks_remaining != i_fighter.stocks_remaining)
      {
        // This might actually happen sometimes if a desync happens right as someone is KO'd...
        // should be quite rare though in a 1v1 situation.
        ERROR_LOG_FMT(SLIPPI_ONLINE,
                      "Stocks remaining for desync recovery do not match: [Player {}] {}, {}",
                      j + 1, fighter.stocks_remaining, i_fighter.stocks_remaining);
        result.is_error = true;
        return result;
      }

      if (abs(static_cast<int>(fighter.current_health) -
              static_cast<int>(i_fighter.current_health)) > 25)
      {
        ERROR_LOG_FMT(SLIPPI_ONLINE,
                      "Current health for desync recovery too different: [Player {}] {}, {}", j + 1,
                      fighter.current_health, i_fighter.current_health);
        result.is_error = true;
        return result;
      }

      // Use the lower health value
      if (i_fighter.current_health < fighter.current_health)
      {
        result.state.fighters[i].current_health = i_fighter.current_health;
      }
    }
  }

  return result;
}
