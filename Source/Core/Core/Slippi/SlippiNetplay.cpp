// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Slippi/SlippiNetplay.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/ENetUtil.h"
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

  if (g_MainNetHost.get() == m_client)
  {
    g_MainNetHost.release();
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
                                         const u8 remotePlayerCount, const u16 localPort,
                                         bool isDecider, u8 playerIdx)
#ifdef _WIN32
    : m_qos_handle(nullptr), m_qos_flow_id(0)
#endif
{
  WARN_LOG_FMT(SLIPPI_ONLINE, "Initializing Slippi Netplay for port: {}, with host: {}", localPort,
               isDecider ? "true" : "false");

  this->isDecider = isDecider;
  this->m_remotePlayerCount = remotePlayerCount;
  this->m_player_idx = playerIdx;

  // Set up remote player data structures.
  int j = 0;
  for (int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++, j++)
  {
    if (j == m_player_idx)
      j++;
    this->matchInfo.remotePlayerSelections[i] = SlippiPlayerSelections();
    this->matchInfo.remotePlayerSelections[i].playerIdx = j;

    this->remotePadQueue[i] = std::deque<std::unique_ptr<SlippiPad>>();
    this->frameOffsetData[i] = FrameOffsetData();
    this->lastFrameTiming[i] = FrameTiming();
    this->pingUs[i] = 0;
    this->lastFrameAcked[i] = 0;
  }

  SLIPPI_NETPLAY = std::move(this);

  // Local address
  ENetAddress* localAddr = nullptr;
  ENetAddress localAddrDef;

  // It is important to be able to set the local port to listen on even in a client connection
  // because not doing so will break hole punching, the host is expecting traffic to come from a
  // specific ip/port and if the port does not match what it is expecting, it will not get through
  // the NAT on some routers
  if (localPort > 0)
  {
    INFO_LOG_FMT(SLIPPI_ONLINE, "Setting up local address");

    localAddrDef.host = ENET_HOST_ANY;
    localAddrDef.port = localPort;

    localAddr = &localAddrDef;
  }

  // TODO: Figure out how to use a local port when not hosting without accepting incoming
  // connections
  m_client = enet_host_create(localAddr, 10, 3, 0, 0);

  if (m_client == nullptr)
  {
    PanicAlertFmtT("Couldn't Create Client");
  }

  for (int i = 0; i < remotePlayerCount; i++)
  {
    ENetAddress addr;
    enet_address_set_host(&addr, addrs[i].c_str());
    addr.port = ports[i];
    // INFO_LOG_FMT(SLIPPI_ONLINE, "Set ENet host, addr = {}, port = {}", addr.host, addr.port);

    ENetPeer* peer = enet_host_connect(m_client, &addr, 3, 0);
    m_server.push_back(peer);

    // Store this connection
    std::stringstream keyStrm;
    keyStrm << addr.host << "-" << addr.port;
    activeConnections[keyStrm.str()][peer] = true;
    INFO_LOG_FMT(SLIPPI_ONLINE, "New connection (constr): {}", keyStrm.str());

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

  slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED;

  m_thread = std::thread(&SlippiNetplayClient::ThreadFunc, this);
}

// Make a dummy client
SlippiNetplayClient::SlippiNetplayClient(bool isDecider)
{
  this->isDecider = isDecider;
  SLIPPI_NETPLAY = std::move(this);
  slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
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
    u64 curTime = Common::Timer::NowUs();

    s32 frame;
    s32 checksum_frame;
    u32 checksum;

    if (!(packet >> frame))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay packet too small to read frame count");
      break;
    }

    u8 packetPlayerPort;
    if (!(packet >> packetPlayerPort))
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
    u8 pIdx = PlayerIdxFromPort(packetPlayerPort);
    if (pIdx >= m_remotePlayerCount)
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Got packet with invalid player idx {}", pIdx);
      break;
    }

    // This is the amount of bytes from the start of the packet where the pad data starts
    int pad_data_offset = 14;

    // This fetches the m_server index that stores the connection we want to overwrite (if
    // necessary). Note that this index is not necessarily the same as the pIdx because if we have
    // users connecting with the same WAN, the m_server indices might not match
    int connIdx = 0;
    for (int i = 0; i < m_server.size(); i++)
    {
      if (peer->address.host == m_server[i]->address.host &&
          peer->address.port == m_server[i]->address.port)
      {
        connIdx = i;
        break;
      }
    }

    // Here we check if we have more than 1 connection for a specific player, this can happen
    // because both players try to connect to each other at the same time to increase the odds that
    // one direction might work and for hole punching. That said there's no point in keeping more
    // than 1 connection alive. I think they might use bandwidth with keep alives or something. Only
    // the lower port player will initiate the disconnect
    std::stringstream keyStrm;
    keyStrm << peer->address.host << "-" << peer->address.port;
    if (activeConnections[keyStrm.str()].size() > 1 && m_player_idx <= pIdx)
    {
      m_server[connIdx] = peer;
      INFO_LOG_FMT(
          SLIPPI_ONLINE,
          "Multiple connections detected for single peer. {}:{}. Disconnecting superfluous "
          "connections. oppIdx: {}. pIdx: {}",
          peer->address.host, peer->address.port, pIdx, m_player_idx);

      for (auto activeConn : activeConnections[keyStrm.str()])
      {
        if (activeConn.first == peer)
          continue;

        // Tell our peer to terminate this connection
        enet_peer_disconnect(activeConn.first, 0);
      }
    }

    // Pad received, try to guess what our local time was when the frame was sent by our opponent
    // before we initialized
    // We can compare this to when we sent a pad for last frame to figure out how far/behind we
    // are with respect to the opponent

    auto timing = lastFrameTiming[pIdx];
    if (!hasGameStarted)
    {
      // Handle case where opponent starts sending inputs before our game has reached frame 1. This
      // will continuously say frame 0 is now to prevent opp from getting too far ahead
      timing.frame = 0;
      timing.timeUs = curTime;
    }

    s64 opponentSendTimeUs = curTime - (pingUs[pIdx] / 2);
    s64 frameDiffOffsetUs = 16683 * (timing.frame - frame);
    s64 timeOffsetUs = opponentSendTimeUs - timing.timeUs + frameDiffOffsetUs;

    // INFO_LOG_FMT(SLIPPI_ONLINE, "[Offset] Opp Frame: {}, My Frame: {}. Time offset: {}", frame,
    //              timing.frame, timeOffsetUs);

    // Add this offset to circular buffer for use later
    if (frameOffsetData[pIdx].buf.size() < SLIPPI_ONLINE_LOCKSTEP_INTERVAL)
      frameOffsetData[pIdx].buf.push_back(static_cast<s32>(timeOffsetUs));
    else
      frameOffsetData[pIdx].buf[frameOffsetData[pIdx].idx] = (s32)timeOffsetUs;

    frameOffsetData[pIdx].idx = (frameOffsetData[pIdx].idx + 1) % SLIPPI_ONLINE_LOCKSTEP_INTERVAL;

    s64 inputs_to_copy;
    {
      std::lock_guard<std::mutex> lk(pad_mutex);  // TODO: Is this the correct lock?

      auto packetData = (u8*)packet.getData();

      // INFO_LOG_FMT(SLIPPI_ONLINE, "Receiving a packet of inputs [{}]...", frame);

      // INFO_LOG_FMT(SLIPPI_ONLINE, "Receiving a packet of inputs from player {}({}) [{}]...",
      //              packetPlayerPort, pIdx, frame);

      s64 frame64 = static_cast<s64>(frame);
      s32 headFrame = remotePadQueue[pIdx].empty() ? 0 : remotePadQueue[pIdx].front()->frame;
      s64 inputs_to_copy = frame64 - static_cast<s64>(headFrame);

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
            std::make_unique<SlippiPad>(static_cast<s32>(frame64 - i), pIdx,
                                        &packetData[pad_data_offset + i * SLIPPI_PAD_DATA_SIZE]);
        remotePadQueue[pIdx].push_front(std::move(pad));
      }
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
      // frame, packetPlayerPort,
      //         peer->address.host, peer->address.port);

      ENetPacket* epac =
          enet_packet_create(spac.getData(), spac.getDataSize(), ENET_PACKET_FLAG_UNSEQUENCED);
      int sendResult = enet_peer_send(peer, 2, epac);
    }
  }
  break;

  case NetPlay::MessageID::SLIPPI_PAD_ACK:
  {
    std::lock_guard<std::mutex> lk(ack_mutex);  // Trying to fix rare crash on ackTimers.count

    // Store last frame acked
    int32_t frame;
    if (!(packet >> frame))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Ack packet too small to read frame");
      break;
    }

    u8 packetPlayerPort;
    if (!(packet >> packetPlayerPort))
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Netplay ack packet too small to read player index");
      break;
    }
    u8 pIdx = PlayerIdxFromPort(packetPlayerPort);
    if (pIdx >= m_remotePlayerCount)
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Got ack packet with invalid player idx {}", pIdx);
      break;
    }

    // INFO_LOG_FMT(SLIPPI_ONLINE, "Received ack packet from player {}({}) [{}]...",
    // packetPlayerPort,
    //              pIdx, frame);

    lastFrameAcked[pIdx] = frame > lastFrameAcked[pIdx] ? frame : lastFrameAcked[pIdx];

    // Remove old timings
    while (!ackTimers[pIdx].empty() && ackTimers[pIdx].front().frame < frame)
    {
      ackTimers[pIdx].pop();
    }

    // Don't get a ping if we do not have the right ack frame
    if (ackTimers[pIdx].empty() || ackTimers[pIdx].front().frame != frame)
    {
      break;
    }

    auto sendTime = ackTimers[pIdx].front().timeUs;
    ackTimers[pIdx].pop();

    pingUs[pIdx] = Common::Timer::NowUs() - sendTime;
    if (g_ActiveConfig.bShowNetPlayPing && frame % SLIPPI_PING_DISPLAY_INTERVAL == 0 && pIdx == 0)
    {
      std::stringstream pingDisplay;
      pingDisplay << "Ping: " << (pingUs[0] / 1000);
      for (int i = 1; i < m_remotePlayerCount; i++)
      {
        pingDisplay << " | " << (pingUs[i] / 1000);
      }
      OSD::AddTypedMessage(OSD::MessageType::NetPlayPing, pingDisplay.str(), OSD::Duration::NORMAL,
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
                   s->playerIdx);
      u8 idx = PlayerIdxFromPort(s->playerIdx);
      if (idx >= m_remotePlayerCount)
      {
        ERROR_LOG_FMT(SLIPPI_ONLINE, "Got match selection packet with invalid player idx {}", idx);
        break;
      }
      matchInfo.remotePlayerSelections[idx].Merge(*s);

      // This might be a good place to reset some logic? Game can't start until we receive this msg
      // so this should ensure that everything is initialized before the game starts
      hasGameStarted = false;

      // Reset remote pad queue such that next inputs that we get are not compared to inputs from
      // last game
      remotePadQueue[idx].clear();
    }
  }
  break;

  case NetPlay::MessageID::SLIPPI_CHAT_MESSAGE:
  {
    auto playerSelection = ReadChatMessageFromPacket(packet);
    INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Received chat message from opponent {}: {}",
                 playerSelection->playerIdx, playerSelection->messageId);

    if (!playerSelection->error)
    {
      // set message id to netplay instance
      remoteChatMessageSelection = std::move(playerSelection);
    }
  }
  break;

  case NetPlay::MessageID::SLIPPI_CONN_SELECTED:
  {
    // Currently this is unused but the intent is to support two-way simultaneous connection
    // attempts
    isConnectionSelected = true;
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

  default:
    WARN_LOG_FMT(SLIPPI_ONLINE, "Unknown message received with id : {}", static_cast<u8>(mid));
    break;
  }

  return 0;
}

void SlippiNetplayClient::writeToPacket(sf::Packet& packet, SlippiPlayerSelections& s)
{
  packet << static_cast<u8>(NetPlay::MessageID::SLIPPI_MATCH_SELECTIONS);
  packet << s.characterId << s.characterColor << s.isCharacterSelected;
  packet << s.playerIdx;
  packet << s.stageId << s.isStageSelected;
  packet << s.rngOffset;
  packet << s.teamId;
}

void SlippiNetplayClient::WriteChatMessageToPacket(sf::Packet& packet, int messageId, u8 player_id)
{
  packet << static_cast<u8>(NetPlay::MessageID::SLIPPI_CHAT_MESSAGE);
  packet << messageId;
  packet << player_id;
}

std::unique_ptr<SlippiPlayerSelections>
SlippiNetplayClient::ReadChatMessageFromPacket(sf::Packet& packet)
{
  auto s = std::make_unique<SlippiPlayerSelections>();

  if (!(packet >> s->messageId))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Chat packet too small to read message ID");
    s->error = true;
    return std::move(s);
  }
  if (!(packet >> s->playerIdx))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Chat packet too small to read player index");
    s->error = true;
    return std::move(s);
  }

  switch (s->messageId)
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
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid chat message index: {}", s->messageId);
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

  if (!(packet >> s->characterId))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->characterColor))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->isCharacterSelected))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->playerIdx))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->stageId))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->isStageSelected))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->rngOffset))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }
  if (!(packet >> s->teamId))
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Received invalid player selection");
    s->error = true;
  }

  return std::move(s);
}

void SlippiNetplayClient::Send(sf::Packet& packet)
{
  enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
  u8 channelId = 0;

  for (int i = 0; i < m_server.size(); i++)
  {
    NetPlay::MessageID mid{((u8*)packet.getData())[0]};
    if (mid == NetPlay::MessageID::SLIPPI_PAD || mid == NetPlay::MessageID::SLIPPI_PAD_ACK)
    {
      // Slippi communications do not need reliable connection and do not need to
      // be received in order. Channel is changed so that other reliable communications
      // do not block anything. This may not be necessary if order is not maintained?
      flags = ENET_PACKET_FLAG_UNSEQUENCED;
      channelId = 1;
    }

    ENetPacket* epac = enet_packet_create(packet.getData(), packet.getDataSize(), flags);
    enet_peer_send(m_server[i], channelId, epac);
  }
}

void SlippiNetplayClient::Disconnect()
{
  ENetEvent netEvent;
  slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
  if (activeConnections.empty())
    return;

  for (auto conn : activeConnections)
  {
    for (auto peer : conn.second)
    {
      INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Disconnecting peer {}", peer.first->address.port);
      enet_peer_disconnect(peer.first, 0);
    }
  }

  while (enet_host_service(m_client, &netEvent, 3000) > 0)
  {
    switch (netEvent.type)
    {
    case ENET_EVENT_TYPE_RECEIVE:
      enet_packet_destroy(netEvent.packet);
      break;
    case ENET_EVENT_TYPE_DISCONNECT:
      INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Got disconnect from peer {}",
                   netEvent.peer->address.port);
      break;
    default:
      break;
    }
  }
  // didn't disconnect gracefully force disconnect
  for (auto conn : activeConnections)
  {
    for (auto peer : conn.second)
    {
      enet_peer_reset(peer.first);
    }
  }
  activeConnections.clear();
  m_server.clear();
  SLIPPI_NETPLAY = nullptr;
}

void SlippiNetplayClient::SendAsync(std::unique_ptr<sf::Packet> packet)
{
  {
    std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
    m_async_queue.emplace(std::move(packet));
  }
  ENetUtil::WakeupThread(m_client);
}

// called from ---NETPLAY--- thread
void SlippiNetplayClient::ThreadFunc()
{
  // Let client die 1 second before host such that after a swap, the client won't be connected to
  u64 startTime = Common::Timer::NowMs();
  u64 timeout = 8000;

  std::vector<bool> connections;
  std::vector<ENetAddress> remoteAddrs;
  for (int i = 0; i < m_remotePlayerCount; i++)
  {
    remoteAddrs.push_back(m_server[i]->address);
    connections.push_back(false);
  }

  while (slippiConnectStatus == SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED)
  {
    // This will confirm that connection went through successfully
    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, 500);
    if (net > 0)
    {
      sf::Packet rpac;
      switch (netEvent.type)
      {
      case ENET_EVENT_TYPE_RECEIVE:
        if (!netEvent.peer)
        {
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got receive event with nil peer");
          continue;
        }
        INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got receive event with peer addr {}:{}",
                     netEvent.peer->address.host, netEvent.peer->address.port);
        rpac.append(netEvent.packet->data, netEvent.packet->dataLength);

        OnData(rpac, netEvent.peer);

        enet_packet_destroy(netEvent.packet);
        break;

      case ENET_EVENT_TYPE_DISCONNECT:
        if (!netEvent.peer)
        {
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got disconnect event with nil peer");
          continue;
        }
        INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got disconnect event with peer addr {}:{}",
                     netEvent.peer->address.host, netEvent.peer->address.port);
        break;

      case ENET_EVENT_TYPE_CONNECT:
      {
        if (!netEvent.peer)
        {
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got connect event with nil peer");
          continue;
        }

        std::stringstream keyStrm;
        keyStrm << netEvent.peer->address.host << "-" << netEvent.peer->address.port;
        activeConnections[keyStrm.str()][netEvent.peer] = true;
        INFO_LOG_FMT(SLIPPI_ONLINE, "New connection (early): {}", keyStrm.str().c_str());

        INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] got connect event with peer addr {}:{}",
                     netEvent.peer->address.host, netEvent.peer->address.port);

        auto isAlreadyConnected = false;
        for (int i = 0; i < m_server.size(); i++)
        {
          if (connections[i] && netEvent.peer->address.host == m_server[i]->address.host &&
              netEvent.peer->address.port == m_server[i]->address.port)
          {
            m_server[i] = netEvent.peer;
            isAlreadyConnected = true;
            break;
          }
        }

        if (isAlreadyConnected)
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
                       static_cast<u32>(remoteAddrs[i].host),
                       static_cast<u32>(netEvent.peer->address.host));
          if (remoteAddrs[i].host == netEvent.peer->address.host && !connections[i])
          {
            INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Overwriting ENetPeer for address: {}:{}",
                         static_cast<u32>(netEvent.peer->address.host),
                         static_cast<u32>(netEvent.peer->address.port));
            INFO_LOG_FMT(SLIPPI_ONLINE,
                         "[Netplay] Overwriting ENetPeer with id ({}) with new peer of id {}",
                         static_cast<u32>(m_server[i]->connectID),
                         static_cast<u32>(netEvent.peer->connectID));
            m_server[i] = netEvent.peer;
            connections[i] = true;
            break;
          }
        }
        break;
      }
      }
    }

    bool allConnected = true;
    for (int i = 0; i < m_remotePlayerCount; i++)
    {
      if (!connections[i])
        allConnected = false;
    }

    if (allConnected)
    {
      m_client->intercept = ENetUtil::InterceptCallback;
      INFO_LOG_FMT(SLIPPI_ONLINE, "Slippi online connection successful!");
      slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED;
      break;
    }

    for (int i = 0; i < m_remotePlayerCount; i++)
    {
      INFO_LOG_FMT(SLIPPI_ONLINE, "m_client peer {} state: {}", i,
                   static_cast<u8>(m_client->peers[i].state));
    }
    INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Not yet connected. Res: {}, Type: {}", net,
                 static_cast<u8>(netEvent.type));

    // Time out after enough time has passed
    u64 curTime = Common::Timer::NowMs();
    if ((curTime - startTime) >= timeout || !m_do_loop.IsSet())
    {
      for (int i = 0; i < m_remotePlayerCount; i++)
      {
        if (!connections[i])
        {
          failedConnections.push_back(i);
        }
      }

      slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
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
    ENetEvent netEvent;
    int net;
    net = enet_host_service(m_client, &netEvent, 250);
    while (!m_async_queue.empty())
    {
      Send(*(m_async_queue.front().get()));
      m_async_queue.pop();
    }
    if (net > 0)
    {
      sf::Packet rpac;
      switch (netEvent.type)
      {
      case ENET_EVENT_TYPE_RECEIVE:
      {
        rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
        OnData(rpac, netEvent.peer);
        enet_packet_destroy(netEvent.packet);
        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT:
      {
        std::stringstream keyStrm;
        keyStrm << netEvent.peer->address.host << "-" << netEvent.peer->address.port;
        activeConnections[keyStrm.str()].erase(netEvent.peer);

        // Check to make sure this address+port are one of the ones we are actually connected to.
        // When connecting to someone that randomizes ports, you can get one valid connection from
        // one port and a failed connection on another port. We don't want to cause a real
        // disconnect if we receive a disconnect message from the port we never connected to
        bool isConnectedClient = false;
        for (int i = 0; i < m_server.size(); i++)
        {
          if (netEvent.peer->address.host == m_server[i]->address.host &&
              netEvent.peer->address.port == m_server[i]->address.port)
          {
            isConnectedClient = true;
            break;
          }
        }

        INFO_LOG_FMT(
            SLIPPI_ONLINE,
            "[Netplay] Disconnect late {}:{}. Remaining connections: {}. Is connected client: {}",
            netEvent.peer->address.host, netEvent.peer->address.port,
            activeConnections[keyStrm.str()].size(), isConnectedClient ? "true" : "false");

        // If the disconnect event doesn't come from the client we are actually listening to,
        // it can be safely ignored
        if (isConnectedClient && activeConnections[keyStrm.str()].empty())
        {
          INFO_LOG_FMT(SLIPPI_ONLINE, "[Netplay] Final disconnect received for a client.");
          m_do_loop.Clear();  // Stop the loop, will trigger a disconnect
        }
        break;
      }
      case ENET_EVENT_TYPE_CONNECT:
      {
        std::stringstream keyStrm;
        keyStrm << netEvent.peer->address.host << "-" << netEvent.peer->address.port;
        activeConnections[keyStrm.str()][netEvent.peer] = true;
        INFO_LOG_FMT(SLIPPI_ONLINE, "New connection (late): {}", keyStrm.str().c_str());
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
  return isDecider;
}

bool SlippiNetplayClient::IsConnectionSelected()
{
  return isConnectionSelected;
}

SlippiNetplayClient::SlippiConnectStatus SlippiNetplayClient::GetSlippiConnectStatus()
{
  return slippiConnectStatus;
}

std::vector<int> SlippiNetplayClient::GetFailedConnections()
{
  return failedConnections;
}

void SlippiNetplayClient::StartSlippiGame()
{
  // Reset variables to start a new game
  hasGameStarted = false;

  localPadQueue.clear();

  for (int i = 0; i < m_remotePlayerCount; i++)
  {
    FrameTiming timing;
    timing.frame = 0;
    timing.timeUs = Common::Timer::NowUs();
    lastFrameTiming[i] = timing;
    lastFrameAcked[i] = 0;

    // Reset ack timers
    std::queue<SlippiNetplayClient::FrameTiming> empty;
    std::swap(ackTimers[i], empty);
  }

  // Clear game prep queue in case anything is still lingering
  game_prep_step_queue.clear();

  // Reset match info for next game
  matchInfo.Reset();
}

void SlippiNetplayClient::SendConnectionSelected()
{
  isConnectionSelected = true;
  auto spac = std::make_unique<sf::Packet>();
  *spac << static_cast<u8>(NetPlay::MessageID::SLIPPI_CONN_SELECTED);
  SendAsync(std::move(spac));
}
void SlippiNetplayClient::SendSlippiPad(std::unique_ptr<SlippiPad> pad)
{
  auto status = slippiConnectStatus;
  bool connectionFailed =
      status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
  bool connectionDisconnected =
      status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
  if (connectionFailed || connectionDisconnected)
  {
    return;
  }

  if (pad)
  {
    // Add latest local pad report to queue
    localPadQueue.push_front(std::move(pad));
  }

  // Remove pad reports that have been received and acked
  int minAckFrame = lastFrameAcked[0];
  for (int i = 1; i < m_remotePlayerCount; i++)
  {
    if (lastFrameAcked[i] < minAckFrame)
      minAckFrame = lastFrameAcked[i];
  }
  /*INFO_LOG_FMT(SLIPPI_ONLINE,
               "Checking to drop local inputs, oldest frame: {} | minAckFrame: {} | {}, {}, {}",
               localPadQueue.back()->frame, minAckFrame, lastFrameAcked[0], lastFrameAcked[1],
               lastFrameAcked[2]);*/
  while (!localPadQueue.empty() && localPadQueue.back()->frame < minAckFrame)
  {
    /*INFO_LOG_FMT(SLIPPI_ONLINE, "Dropping local input for frame {} from queue",
                 localPadQueue.back()->frame);*/
    localPadQueue.pop_back();
  }

  if (localPadQueue.empty())
  {
    // If pad queue is empty now, there's no reason to send anything
    return;
  }
  auto frame = localPadQueue.front()->frame;
  auto spac = std::make_unique<sf::Packet>();
  *spac << static_cast<u8>(NetPlay::MessageID::SLIPPI_PAD);
  *spac << frame;
  *spac << this->m_player_idx;
  *spac << localPadQueue.front()->checksum_frame;
  *spac << localPadQueue.front()->checksum;

  for (auto it = localPadQueue.begin(); it != localPadQueue.end(); ++it)
    spac->append((*it)->pad_buf, SLIPPI_PAD_DATA_SIZE);  // only transfer 8 bytes per pad

  SendAsync(std::move(spac));
  u64 time = Common::Timer::NowUs();

  hasGameStarted = true;

  for (int i = 0; i < m_remotePlayerCount; i++)
  {
    FrameTiming timing;
    timing.frame = frame;
    timing.timeUs = time;
    lastFrameTiming[i] = timing;

    // Add send time to ack timers
    FrameTiming sendTime;
    sendTime.frame = frame;
    sendTime.timeUs = time;
    ackTimers[i].emplace(sendTime);
  }
}

void SlippiNetplayClient::SetMatchSelections(SlippiPlayerSelections& s)
{
  matchInfo.localPlayerSelections.Merge(s);
  matchInfo.localPlayerSelections.playerIdx = m_player_idx;

  // Send packet containing selections
  auto spac = std::make_unique<sf::Packet>();
  writeToPacket(*spac, matchInfo.localPlayerSelections);
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
  SlippiPlayerSelections copiedSelection = SlippiPlayerSelections();

  if (remoteChatMessageSelection != nullptr && isChatEnabled)
  {
    copiedSelection.messageId = remoteChatMessageSelection->messageId;
    copiedSelection.playerIdx = remoteChatMessageSelection->playerIdx;

    // Clear it out
    remoteChatMessageSelection->messageId = 0;
    remoteChatMessageSelection->playerIdx = 0;
  }
  else
  {
    copiedSelection.messageId = 0;
    copiedSelection.playerIdx = 0;

    // if chat is not enabled, automatically send back a message saying so.
    if (remoteChatMessageSelection != nullptr && !isChatEnabled &&
        (remoteChatMessageSelection->messageId > 0 &&
         remoteChatMessageSelection->messageId != SlippiPremadeText::CHAT_MSG_CHAT_DISABLED))
    {
      auto packet = std::make_unique<sf::Packet>();
      remoteSentChatMessageId = SlippiPremadeText::CHAT_MSG_CHAT_DISABLED;
      WriteChatMessageToPacket(*packet, remoteSentChatMessageId, LocalPlayerPort());
      SendAsync(std::move(packet));
      remoteSentChatMessageId = 0;
      remoteChatMessageSelection = nullptr;
    }
  }

  return copiedSelection;
}

u8 SlippiNetplayClient::GetSlippiRemoteSentChatMessage(bool isChatEnabled)
{
  if (!isChatEnabled)
  {
    return 0;
  }

  u8 copiedMessageId = remoteSentChatMessageId;
  remoteSentChatMessageId = 0;  // Clear it out
  return copiedMessageId;
}

std::unique_ptr<SlippiRemotePadOutput> SlippiNetplayClient::GetFakePadOutput(int frame)
{
  // Used for testing purposes, will ignore the opponent's actual inputs and provide fake
  // ones to trigger rollback scenarios
  std::unique_ptr<SlippiRemotePadOutput> padOutput = std::make_unique<SlippiRemotePadOutput>();

  // Triggers rollback where the first few inputs were correctly predicted
  if (frame % 60 < 5)
  {
    // Return old inputs for a bit
    padOutput->latest_frame = frame - (frame % 60);
    padOutput->data.insert(padOutput->data.begin(), SLIPPI_PAD_FULL_SIZE, 0);
  }
  else if (frame % 60 == 5)
  {
    padOutput->latest_frame = frame;
    // Add 5 frames of 0'd inputs
    padOutput->data.insert(padOutput->data.begin(), 5 * SLIPPI_PAD_FULL_SIZE, 0);

    // Press A button for 2 inputs prior to this frame causing a rollback
    padOutput->data[2 * SLIPPI_PAD_FULL_SIZE] = 1;
  }
  else
  {
    padOutput->latest_frame = frame;
    padOutput->data.insert(padOutput->data.begin(), SLIPPI_PAD_FULL_SIZE, 0);
  }

  return std::move(padOutput);
}

std::unique_ptr<SlippiRemotePadOutput> SlippiNetplayClient::GetSlippiRemotePad(int index,
                                                                               int maxFrameCount)
{
  std::lock_guard<std::mutex> lk(pad_mutex);  // TODO: Is this the correct lock?

  std::unique_ptr<SlippiRemotePadOutput> padOutput = std::make_unique<SlippiRemotePadOutput>();

  if (remotePadQueue[index].empty())
  {
    auto emptyPad = std::make_unique<SlippiPad>(0);

    padOutput->latest_frame = emptyPad->frame;

    auto emptyIt = std::begin(emptyPad->pad_buf);
    padOutput->data.insert(padOutput->data.end(), emptyIt, emptyIt + SLIPPI_PAD_FULL_SIZE);

    return std::move(padOutput);
  }

  int inputCount = 0;

  padOutput->latest_frame = 0;
  padOutput->checksum_frame = remoteChecksum[index].frame;
  padOutput->checksum = remoteChecksum[index].value;

  // Copy inputs from the remote pad queue to the output. We iterate backwards because
  // we want to get the oldest frames possible (will have been cleared to contain the last
  // finalized frame at the back). I think it's very unlikely but I think before we
  // iterated from the front and it's possible the 7 frame limit left out an input the
  // game actually needed.
  for (auto it = remotePadQueue[index].rbegin(); it != remotePadQueue[index].rend(); ++it)
  {
    if ((*it)->frame > padOutput->latest_frame)
      padOutput->latest_frame = (*it)->frame;

    // NOTICE_LOG(SLIPPI_ONLINE, "[%d] (Remote) P%d %08X %08X %08X", (*it)->frame,
    //						index >= playerIdx ? index + 1 : index, Common::swap32(&(*it)->padBuf[0]),
    //						Common::swap32(&(*it)->padBuf[4]), Common::swap32(&(*it)->padBuf[8]));

    auto padIt = std::begin((*it)->pad_buf);
    padOutput->data.insert(padOutput->data.begin(), padIt, padIt + SLIPPI_PAD_FULL_SIZE);

    // Limit max amount of inputs to send
    inputCount++;
    if (inputCount >= maxFrameCount)
      break;
  }

  return std::move(padOutput);
}

void SlippiNetplayClient::DropOldRemoteInputs(int32_t finalizedFrame)
{
  std::lock_guard<std::mutex> lk(pad_mutex);

  for (int i = 0; i < m_remotePlayerCount; i++)
  {
    while (remotePadQueue[i].size() > 1 && remotePadQueue[i].back()->frame < finalizedFrame)
      remotePadQueue[i].pop_back();
  }
}

SlippiMatchInfo* SlippiNetplayClient::GetMatchInfo()
{
  return &matchInfo;
}

int32_t SlippiNetplayClient::GetSlippiLatestRemoteFrame(int maxFrameCount)
{
  // Return the lowest frame among remote queues
  int lowestFrame = 0;
  bool isFrameSet = false;
  for (int i = 0; i < m_remotePlayerCount; i++)
  {
    auto rp = GetSlippiRemotePad(i, maxFrameCount);
    int f = rp->latest_frame;
    if (f < lowestFrame || !isFrameSet)
    {
      lowestFrame = f;
      isFrameSet = true;
    }
  }

  return lowestFrame;
}

// return the smallest time offset among all remote players
s32 SlippiNetplayClient::CalcTimeOffsetUs()
{
  bool empty = true;
  for (int i = 0; i < m_remotePlayerCount; i++)
  {
    if (!frameOffsetData[i].buf.empty())
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
  for (int i = 0; i < m_remotePlayerCount; i++)
  {
    if (frameOffsetData[i].buf.empty())
      continue;

    std::vector<s32> buf;
    std::copy(frameOffsetData[i].buf.begin(), frameOffsetData[i].buf.end(),
              std::back_inserter(buf));

    // TODO: Does this work?
    std::sort(buf.begin(), buf.end());

    int bufSize = (int)buf.size();
    int offset = (int)((1.0f / 3.0f) * bufSize);
    int end = bufSize - offset;

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

  s32 minOffset = offsets.front();
  for (int i = 1; i < offsets.size(); i++)
  {
    if (offsets[i] < minOffset)
      minOffset = offsets[i];
  }

  return minOffset;
}
