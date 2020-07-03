// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Slippi/SlippiNetplay.h"
#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/ENetUtil.h"
#include "Common/MD5.h"
#include "Common/MsgHandler.h"
#include "Common/Timer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/SI.h"
#include "Core/HW/SI_DeviceGCController.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_emu.h"
#include "Core/Movie.h"
#include "InputCommon/GCAdapter.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"
#include <SlippiGame.h>
#include <algorithm>
#include <fstream>
#include <mbedtls/md5.h>
#include <memory>
#include <thread>

static std::mutex pad_mutex;
static std::mutex ack_mutex;

// called from ---GUI--- thread
SlippiNetplayClient::~SlippiNetplayClient()
{
  m_do_loop.Clear();
  if (m_thread.joinable())
    m_thread.join();

  if (m_server)
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

  WARN_LOG(SLIPPI_ONLINE, "Netplay client cleanup complete");
}

// called from ---SLIPPI EXI--- thread
SlippiNetplayClient::SlippiNetplayClient(const std::string& address, const u16 remotePort, const u16 localPort,
  bool isDecider)
#ifdef _WIN32
  : m_qos_handle(nullptr)
  , m_qos_flow_id(0)
#endif
{
  WARN_LOG(SLIPPI_ONLINE, "Initializing Slippi Netplay for port: %d, with host: %s", localPort,
    isDecider ? "true" : "false");

  this->isDecider = isDecider;

  // Local address
  ENetAddress* localAddr = nullptr;
  ENetAddress localAddrDef;

  // It is important to be able to set the local port to listen on even in a client connection because
  // not doing so will break hole punching, the host is expecting traffic to come from a specific ip/port
  // and if the port does not match what it is expecting, it will not get through the NAT on some routers
  if (localPort > 0)
  {
    INFO_LOG(SLIPPI_ONLINE, "Setting up local address");

    localAddrDef.host = ENET_HOST_ANY;
    localAddrDef.port = localPort;

    localAddr = &localAddrDef;
  }

  // TODO: Figure out how to use a local port when not hosting without accepting incoming connections
  m_client = enet_host_create(localAddr, 2, 3, 0, 0);

  if (m_client == nullptr)
  {
    PanicAlertT("Couldn't Create Client");
  }

  ENetAddress addr;
  enet_address_set_host(&addr, address.c_str());
  addr.port = remotePort;

  m_server = enet_host_connect(m_client, &addr, 3, 0);

  if (m_server == nullptr)
  {
    PanicAlertT("Couldn't create peer.");
  }

  slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED;

  m_thread = std::thread(&SlippiNetplayClient::ThreadFunc, this);
}

// Make a dummy client
SlippiNetplayClient::SlippiNetplayClient(bool isDecider)
{
  this->isDecider = isDecider;
  slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
}

// called from ---NETPLAY--- thread
unsigned int SlippiNetplayClient::OnData(sf::Packet& packet)
{
  MessageId mid;
  packet >> mid;

  switch (mid)
  {
  case NP_MSG_SLIPPI_PAD:
  {
    int32_t frame;
    packet >> frame;

    // Pad received, try to guess what our local time was when the frame was sent by our opponent
    // before we initialized
    // We can compare this to when we sent a pad for last frame to figure out how far/behind we
    // are with respect to the opponent

    u64 curTime = Common::Timer::GetTimeUs();

    auto timing = lastFrameTiming;
    if (!hasGameStarted)
    {
      // Handle case where opponent starts sending inputs before our game has reached frame 1. This will
      // continuously say frame 0 is now to prevent opp from getting too far ahead
      timing.frame = 0;
      timing.timeUs = curTime;
    }

    s64 opponentSendTimeUs = curTime - (pingUs / 2);
    s64 frameDiffOffsetUs = 16683 * (timing.frame - frame);
    s64 timeOffsetUs = opponentSendTimeUs - timing.timeUs + frameDiffOffsetUs;

    INFO_LOG(SLIPPI_ONLINE, "[Offset] Opp Frame: %d, My Frame: %d. Time offset: %lld", frame, timing.frame,
      timeOffsetUs);

    // Add this offset to circular buffer for use later
    if (frameOffsetData.buf.size() < SLIPPI_ONLINE_LOCKSTEP_INTERVAL)
      frameOffsetData.buf.push_back((s32)timeOffsetUs);
    else
      frameOffsetData.buf[frameOffsetData.idx] = (s32)timeOffsetUs;

    frameOffsetData.idx = (frameOffsetData.idx + 1) % SLIPPI_ONLINE_LOCKSTEP_INTERVAL;

    {
      std::lock_guard<std::mutex> lk(pad_mutex); // TODO: Is this the correct lock?

      auto packetData = (u8*)packet.getData();

      INFO_LOG(SLIPPI_ONLINE, "Receiving a packet of inputs [%d]...", frame);

      int32_t headFrame = remotePadQueue.empty() ? 0 : remotePadQueue.front()->frame;
      int inputsToCopy = frame - headFrame;
      for (int i = inputsToCopy - 1; i >= 0; i--)
      {
        auto pad = std::make_unique<SlippiPad>(frame - i, &packetData[5 + i * SLIPPI_PAD_DATA_SIZE]);

        INFO_LOG(SLIPPI_ONLINE, "Rcv [%d] -> %02X %02X %02X %02X %02X %02X %02X %02X", pad->frame,
          pad->padBuf[0], pad->padBuf[1], pad->padBuf[2], pad->padBuf[3], pad->padBuf[4], pad->padBuf[5],
          pad->padBuf[6], pad->padBuf[7]);

        remotePadQueue.push_front(std::move(pad));
      }
    }

    // Send Ack
    sf::Packet spac;
    spac << (MessageId)NP_MSG_SLIPPI_PAD_ACK;
    spac << frame;
    Send(spac);
  }
  break;

  case NP_MSG_SLIPPI_PAD_ACK:
  {
    std::lock_guard<std::mutex> lk(ack_mutex); // Trying to fix rare crash on ackTimers.count

    // Store last frame acked
    int32_t frame;
    packet >> frame;

    lastFrameAcked = frame > lastFrameAcked ? frame : lastFrameAcked;

    // Remove old timings
    while (!ackTimers.Empty() && ackTimers.Front().frame < frame)
    {
      ackTimers.Pop();
    }

    // Don't get a ping if we do not have the right ack frame
    if (ackTimers.Empty() || ackTimers.Front().frame != frame)
    {
      break;
    }

    auto sendTime = ackTimers.Front().timeUs;
    ackTimers.Pop();

    pingUs = Common::Timer::GetTimeUs() - sendTime;
    if (g_ActiveConfig.bShowNetPlayPing && frame % SLIPPI_PING_DISPLAY_INTERVAL == 0)
    {
      OSD::AddTypedMessage(OSD::MessageType::NetPlayPing, StringFromFormat("Ping: %u", pingUs / 1000),
        OSD::Duration::NORMAL, OSD::Color::CYAN);
    }
  }
  break;

  case NP_MSG_SLIPPI_MATCH_SELECTIONS:
  {
    auto s = readSelectionsFromPacket(packet);
    ERROR_LOG(SLIPPI_ONLINE, "[Received Selections] Char: 0x%X, Color: 0x%X", s->characterId, s->characterId);
    matchInfo.remotePlayerSelections.Merge(*s);

    // Set player name is not empty
    if (!matchInfo.remotePlayerSelections.playerName.empty())
    {
      oppName = matchInfo.remotePlayerSelections.playerName;
    }

    // This might be a good place to reset some logic? Game can't start until we receive this msg
    // so this should ensure that everything is initialized before the game starts
    // TODO: This could cause issues in the case of a desync? If this is ever received mid-game, bad things
    // TODO: will happen. Consider improving this
    hasGameStarted = false;
  }
  break;

  case NP_MSG_SLIPPI_CONN_SELECTED:
  {
    // Currently this is unused but the intent is to support two-way simultaneous connection attempts
    isConnectionSelected = true;
  }
  break;

  default:
    PanicAlertT("Unknown message received with id : %d", mid);
    break;
  }

  return 0;
}

void SlippiNetplayClient::writeToPacket(sf::Packet& packet, SlippiPlayerSelections& s)
{
  packet << static_cast<MessageId>(NP_MSG_SLIPPI_MATCH_SELECTIONS);
  packet << s.characterId << s.characterColor << s.isCharacterSelected;
  packet << s.stageId << s.isStageSelected;
  packet << s.rngOffset;
  packet << s.playerName;
  packet << s.connectCode;
}

std::unique_ptr<SlippiPlayerSelections> SlippiNetplayClient::readSelectionsFromPacket(sf::Packet& packet)
{
  auto s = std::make_unique<SlippiPlayerSelections>();

  packet >> s->characterId;
  packet >> s->characterColor;
  packet >> s->isCharacterSelected;

  packet >> s->stageId;
  packet >> s->isStageSelected;

  packet >> s->rngOffset;
  packet >> s->playerName;
  packet >> s->connectCode;

  return std::move(s);
}

void SlippiNetplayClient::Send(sf::Packet& packet)
{
  enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
  u8 channelId = 0;

  MessageId mid = ((u8*)packet.getData())[0];
  if (mid == NP_MSG_SLIPPI_PAD || mid == NP_MSG_SLIPPI_PAD_ACK)
  {
    // Slippi communications do not need reliable connection and do not need to
    // be received in order. Channel is changed so that other reliable communications
    // do not block anything. This may not be necessary if order is not maintained?
    flags = ENET_PACKET_FLAG_UNSEQUENCED;
    channelId = 1;
  }

  ENetPacket* epac = enet_packet_create(packet.getData(), packet.getDataSize(), flags);
  enet_peer_send(m_server, channelId, epac);
}

void SlippiNetplayClient::Disconnect()
{
  ENetEvent netEvent;
  slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
  if (m_server)
    enet_peer_disconnect(m_server, 0);
  else
    return;

  while (enet_host_service(m_client, &netEvent, 3000) > 0)
  {
    switch (netEvent.type)
    {
    case ENET_EVENT_TYPE_RECEIVE:
      enet_packet_destroy(netEvent.packet);
      break;
    case ENET_EVENT_TYPE_DISCONNECT:
      m_server = nullptr;
      return;
    default:
      break;
    }
  }
  // didn't disconnect gracefully force disconnect
  enet_peer_reset(m_server);
  m_server = nullptr;
}

void SlippiNetplayClient::SendAsync(std::unique_ptr<sf::Packet> packet)
{
  {
    std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
    m_async_queue.Push(std::move(packet));
  }
  ENetUtil::WakeupThread(m_client);
}

// called from ---NETPLAY--- thread
void SlippiNetplayClient::ThreadFunc()
{
  // Let client die 1 second before host such that after a swap, the client won't be connected to
  int attemptCountLimit = 16;

  int attemptCount = 0;
  while (slippiConnectStatus == SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED)
  {
    // This will confirm that connection went through successfully
    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, 500);
    if (net > 0 && netEvent.type == ENET_EVENT_TYPE_CONNECT)
    {
      // TODO: Confirm gecko codes match?
      if (netEvent.peer)
      {
        WARN_LOG(SLIPPI_ONLINE, "[Netplay] Overwritting server");
        m_server = netEvent.peer;
      }

      m_client->intercept = ENetUtil::InterceptCallback;
      slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED;
      INFO_LOG(SLIPPI_ONLINE, "Slippi online connection successful!");
      break;
    }

    WARN_LOG(SLIPPI_ONLINE, "[Netplay] Not yet connected. Res: %d, Type: %d", net, netEvent.type);

    // Time out after enough time has passed
    attemptCount++;
    if (attemptCount >= attemptCountLimit || !m_do_loop.IsSet())
    {
      slippiConnectStatus = SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
      INFO_LOG(SLIPPI_ONLINE, "Slippi online connection failed");
      return;
    }
  }

  bool qos_success = false;
#ifdef _WIN32
  QOS_VERSION ver = { 1, 0 };

  if (SConfig::GetInstance().bQoSEnabled && QOSCreateHandle(&ver, &m_qos_handle))
  {
    // from win32.c
    struct sockaddr_in sin = { 0 };

    sin.sin_family = AF_INET;
    sin.sin_port = ENET_HOST_TO_NET_16(m_server->host->address.port);
    sin.sin_addr.s_addr = m_server->host->address.host;

    if (QOSAddSocketToFlow(m_qos_handle, m_server->host->socket, reinterpret_cast<PSOCKADDR>(&sin),
      // this is 0x38
      QOSTrafficTypeControl, QOS_NON_ADAPTIVE_FLOW, &m_qos_flow_id))
    {
      DWORD dscp = 0x2e;

      // this will fail if we're not admin
      // sets DSCP to the same as linux (0x2e)
      QOSSetFlow(m_qos_handle, m_qos_flow_id, QOSSetOutgoingDSCPValue, sizeof(DWORD), &dscp, 0, nullptr);

      qos_success = true;
    }
  }
#else
  if (SConfig::GetInstance().bQoSEnabled)
  {
#ifdef __linux__
    // highest priority
    int priority = 7;
    setsockopt(m_server->host->socket, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
#endif

    // https://www.tucny.com/Home/dscp-tos
    // ef is better than cs7
    int tos_val = 0xb8;
    qos_success = setsockopt(m_server->host->socket, IPPROTO_IP, IP_TOS, &tos_val, sizeof(tos_val)) == 0;
  }
#endif

  while (m_do_loop.IsSet())
  {
    ENetEvent netEvent;
    int net;
    net = enet_host_service(m_client, &netEvent, 250);
    while (!m_async_queue.Empty())
    {
      Send(*(m_async_queue.Front().get()));
      m_async_queue.Pop();
    }
    if (net > 0)
    {
      sf::Packet rpac;
      switch (netEvent.type)
      {
      case ENET_EVENT_TYPE_RECEIVE:
        rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
        OnData(rpac);

        enet_packet_destroy(netEvent.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        ERROR_LOG(SLIPPI_ONLINE, "[Netplay] Disconnected Event detected: %s",
          netEvent.peer == m_server ? "same client" : "diff client");

        // If the disconnect event doesn't come from the client we are actually listening to,
        // it can be safely ignored
        if (netEvent.peer == m_server)
        {
          m_do_loop.Clear(); // Stop the loop, will trigger a disconnect
        }
        break;
      default:
        break;
      }
    }
  }

#ifdef _WIN32
  if (m_qos_handle != 0)
  {
    if (m_qos_flow_id != 0)
      QOSRemoveSocketFromFlow(m_qos_handle, m_server->host->socket, m_qos_flow_id, 0);
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

void SlippiNetplayClient::StartSlippiGame()
{
  // Reset variables to start a new game
  lastFrameAcked = 0;

  FrameTiming timing;
  timing.frame = 0;
  timing.timeUs = Common::Timer::GetTimeUs();
  lastFrameTiming = timing;
  hasGameStarted = false;

  localPadQueue.clear();

  remotePadQueue.clear();
  for (s32 i = 1; i <= 2; i++)
  {
    std::unique_ptr<SlippiPad> pad = std::make_unique<SlippiPad>(i);
    remotePadQueue.push_front(std::move(pad));
  }

  // Reset match info for next game
  matchInfo.Reset();

  // Reset ack timers
  ackTimers.Clear();
}

void SlippiNetplayClient::SendConnectionSelected()
{
  isConnectionSelected = true;

  auto spac = std::make_unique<sf::Packet>();
  *spac << static_cast<MessageId>(NP_MSG_SLIPPI_CONN_SELECTED);
  SendAsync(std::move(spac));
}

void SlippiNetplayClient::SendSlippiPad(std::unique_ptr<SlippiPad> pad)
{
  auto status = slippiConnectStatus;
  bool connectionFailed = status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
  bool connectionDisconnected = status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
  if (connectionFailed || connectionDisconnected)
  {
    return;
  }

  // if (pad && isDecider)
  //{
  //  ERROR_LOG(SLIPPI_ONLINE, "[%d] %X %X %X %X %X %X %X %X", pad->frame, pad->padBuf[0], pad->padBuf[1],
  //  pad->padBuf[2], pad->padBuf[3], pad->padBuf[4], pad->padBuf[5], pad->padBuf[6], pad->padBuf[7]);
  //}

  if (pad)
  {
    // Add latest local pad report to queue
    localPadQueue.push_front(std::move(pad));
  }

  // Remove pad reports that have been received and acked
  while (!localPadQueue.empty() && localPadQueue.back()->frame < lastFrameAcked)
  {
    localPadQueue.pop_back();
  }

  if (localPadQueue.empty())
  {
    // If pad queue is empty now, there's no reason to send anything
    return;
  }

  auto frame = localPadQueue.front()->frame;

  auto spac = std::make_unique<sf::Packet>();
  *spac << static_cast<MessageId>(NP_MSG_SLIPPI_PAD);
  *spac << frame;

  INFO_LOG(SLIPPI_ONLINE, "Sending a packet of inputs [%d]...", frame);
  for (auto it = localPadQueue.begin(); it != localPadQueue.end(); ++it)
  {
    INFO_LOG(SLIPPI_ONLINE, "Send [%d] -> %02X %02X %02X %02X %02X %02X %02X %02X", (*it)->frame, (*it)->padBuf[0],
      (*it)->padBuf[1], (*it)->padBuf[2], (*it)->padBuf[3], (*it)->padBuf[4], (*it)->padBuf[5],
      (*it)->padBuf[6], (*it)->padBuf[7]);
    spac->append((*it)->padBuf, SLIPPI_PAD_DATA_SIZE); // only transfer 8 bytes per pad
  }

  SendAsync(std::move(spac));

  u64 time = Common::Timer::GetTimeUs();

  hasGameStarted = true;

  FrameTiming timing;
  timing.frame = frame;
  timing.timeUs = time;
  lastFrameTiming = timing;

  // Add send time to ack timers
  FrameTiming sendTime;
  sendTime.frame = frame;
  sendTime.timeUs = time;
  ackTimers.Push(sendTime);
}

void SlippiNetplayClient::SetMatchSelections(SlippiPlayerSelections& s)
{
  matchInfo.localPlayerSelections.Merge(s);

  // Send packet containing selections
  auto spac = std::make_unique<sf::Packet>();
  writeToPacket(*spac, matchInfo.localPlayerSelections);
  SendAsync(std::move(spac));
}

std::unique_ptr<SlippiRemotePadOutput> SlippiNetplayClient::GetSlippiRemotePad(int32_t curFrame)
{
  std::lock_guard<std::mutex> lk(pad_mutex); // TODO: Is this the correct lock?

  std::unique_ptr<SlippiRemotePadOutput> padOutput = std::make_unique<SlippiRemotePadOutput>();

  if (remotePadQueue.empty())
  {
    auto emptyPad = std::make_unique<SlippiPad>(0);

    padOutput->latestFrame = emptyPad->frame;

    auto emptyIt = std::begin(emptyPad->padBuf);
    padOutput->data.insert(padOutput->data.end(), emptyIt, emptyIt + SLIPPI_PAD_FULL_SIZE);

    return std::move(padOutput);
  }

  padOutput->latestFrame = remotePadQueue.front()->frame;

  // Copy the entire remaining remote buffer
  for (auto it = remotePadQueue.begin(); it != remotePadQueue.end(); ++it)
  {
    auto padIt = std::begin((*it)->padBuf);
    padOutput->data.insert(padOutput->data.end(), padIt, padIt + SLIPPI_PAD_FULL_SIZE);
  }

  // Remove pad reports that should no longer be needed
  while (remotePadQueue.size() > 1 && remotePadQueue.back()->frame < curFrame)
  {
    remotePadQueue.pop_back();
  }

  return std::move(padOutput);
}

SlippiMatchInfo* SlippiNetplayClient::GetMatchInfo()
{
  return &matchInfo;
}

u64 SlippiNetplayClient::GetSlippiPing()
{
  return pingUs;
}

std::string SlippiNetplayClient::GetOpponentName()
{
  return oppName;
}

int32_t SlippiNetplayClient::GetSlippiLatestRemoteFrame()
{
  std::lock_guard<std::mutex> lk(pad_mutex); // TODO: Is this the correct lock?

  if (remotePadQueue.empty())
  {
    return 0;
  }

  return remotePadQueue.front()->frame;
}

s32 SlippiNetplayClient::CalcTimeOffsetUs()
{
  if (frameOffsetData.buf.empty())
  {
    return 0;
  }

  std::vector<s32> buf;
  std::copy(frameOffsetData.buf.begin(), frameOffsetData.buf.end(), std::back_inserter(buf));

  // TODO: Does this work?
  std::sort(buf.begin(), buf.end());

  int bufSize = (int)buf.size();
  int offset = (int)((1.0f / 3.0f) * bufSize);
  int end = bufSize - offset;

  int sum = 0;
  for (int i = offset; i < end; i++)
  {
    sum += buf[i];
  }

  int count = end - offset;
  if (count <= 0)
  {
    return 0; // What do I return here?
  }

  return sum / count;
}
