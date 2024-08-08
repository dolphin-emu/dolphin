// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/NetPlayClient.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/ENet.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/QoSSession.h"
#include "Common/SFMLHelper.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "Common/Version.h"

#include "Core/ActionReplay.h"
#include "Core/Boot/Boot.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Config/SessionSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#ifdef HAS_LIBMGBA
#include "Core/HW/GBACore.h"
#endif
#include "Core/HW/GBAPad.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SI/SI_DeviceGCController.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiiSave.h"
#include "Core/HW/WiiSaveStructs.h"
#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/FS/HostBackend/FS.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/Uids.h"
#include "Core/Movie.h"
#include "Core/NetPlayCommon.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/SyncIdentifier.h"
#include "Core/System.h"
#include "DiscIO/Blob.h"

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/GCAdapter.h"
#include "InputCommon/InputConfig.h"
#include "UICommon/GameFile.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

namespace NetPlay
{
using namespace WiimoteCommon;

static std::mutex crit_netplay_client;
static NetPlayClient* netplay_client = nullptr;
static bool s_si_poll_batching = false;

// called from ---GUI--- thread
NetPlayClient::~NetPlayClient()
{
  // not perfect
  if (m_is_running.IsSet())
    StopGame();

  if (m_is_connected)
  {
    m_should_compute_game_digest = false;
    m_dialog->AbortGameDigest();
    if (m_game_digest_thread.joinable())
      m_game_digest_thread.join();
    m_do_loop.Clear();
    m_thread.join();

    m_chunked_data_receive_queue.clear();
    m_dialog->HideChunkedProgressDialog();
  }

  if (m_server)
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

  if (m_traversal_client)
  {
    Common::ReleaseTraversalClient();
  }
}

// called from ---GUI--- thread
NetPlayClient::NetPlayClient(const std::string& address, const u16 port, NetPlayUI* dialog,
                             const std::string& name, const NetTraversalConfig& traversal_config)
    : m_dialog(dialog), m_player_name(name)
{
  ClearBuffers();

  if (!traversal_config.use_traversal)
  {
    // Direct Connection
    m_client = enet_host_create(nullptr, 1, CHANNEL_COUNT, 0, 0);

    if (m_client == nullptr)
    {
      m_dialog->OnConnectionError(_trans("Could not create client."));
      return;
    }

    m_client->mtu = std::min(m_client->mtu, NetPlay::MAX_ENET_MTU);

    ENetAddress addr;
    enet_address_set_host(&addr, address.c_str());
    addr.port = port;

    m_server = enet_host_connect(m_client, &addr, CHANNEL_COUNT, 0);

    if (m_server == nullptr)
    {
      m_dialog->OnConnectionError(_trans("Could not create peer."));
      return;
    }

    // Update time in milliseconds of no acknoledgment of
    // sent packets before a connection is deemed disconnected
    enet_peer_timeout(m_server, 0, PEER_TIMEOUT.count(), PEER_TIMEOUT.count());

    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, 5000);
    if (net > 0 && netEvent.type == ENET_EVENT_TYPE_CONNECT)
    {
      if (Connect())
      {
        m_client->intercept = Common::ENet::InterceptCallback;
        m_thread = std::thread(&NetPlayClient::ThreadFunc, this);
      }
    }
    else
    {
      m_dialog->OnConnectionError(_trans("Could not communicate with host."));
    }
  }
  else
  {
    if (address.size() > Common::NETPLAY_CODE_SIZE)
    {
      m_dialog->OnConnectionError(
          _trans("The host code is too long.\nPlease recheck that you have the correct code."));
      return;
    }

    if (!Common::EnsureTraversalClient(traversal_config.traversal_host,
                                       traversal_config.traversal_port))
    {
      return;
    }
    m_client = Common::g_MainNetHost.get();

    m_traversal_client = Common::g_TraversalClient.get();

    // If we were disconnected in the background, reconnect.
    if (m_traversal_client->HasFailed())
      m_traversal_client->ReconnectToServer();
    m_traversal_client->m_Client = this;
    m_host_spec = address;
    m_connection_state = ConnectionState::WaitingForTraversalClientConnection;
    OnTraversalStateChanged();
    m_connecting = true;

    Common::Timer connect_timer;
    connect_timer.Start();

    while (m_connecting)
    {
      ENetEvent netEvent;
      if (m_traversal_client)
        m_traversal_client->HandleResends();

      while (enet_host_service(m_client, &netEvent, 4) > 0)
      {
        sf::Packet rpac;
        switch (netEvent.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
          m_server = netEvent.peer;

          // Update time in milliseconds of no acknoledgment of
          // sent packets before a connection is deemed disconnected
          enet_peer_timeout(m_server, 0, PEER_TIMEOUT.count(), PEER_TIMEOUT.count());

          if (Connect())
          {
            m_connection_state = ConnectionState::Connected;
            m_thread = std::thread(&NetPlayClient::ThreadFunc, this);
          }
          return;
        default:
          break;
        }
      }
      if (connect_timer.ElapsedMs() > 5000)
        break;
    }
    m_dialog->OnConnectionError(_trans("Could not communicate with host."));
  }
}

bool NetPlayClient::Connect()
{
  INFO_LOG_FMT(NETPLAY, "Connecting to server.");

  // send connect message
  sf::Packet packet;
  packet << Common::GetScmRevGitStr();
  packet << Common::GetNetplayDolphinVer();
  packet << m_player_name;
  Send(packet);
  enet_host_flush(m_client);
  sf::Packet rpac;
  // TODO: make this not hang
  ENetEvent netEvent;
  int net;
  while ((net = enet_host_service(m_client, &netEvent, 5000)) > 0 &&
         static_cast<int>(netEvent.type) == Common::ENet::SKIPPABLE_EVENT)
  {
    // ignore packets from traversal server
  }
  if (net > 0 && netEvent.type == ENET_EVENT_TYPE_RECEIVE)
  {
    rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
    enet_packet_destroy(netEvent.packet);
  }
  else
  {
    return false;
  }

  ConnectionError error;
  rpac >> error;

  // got error message
  if (error != ConnectionError::NoError)
  {
    switch (error)
    {
    case ConnectionError::ServerFull:
      m_dialog->OnConnectionError(_trans("The server is full."));
      break;
    case ConnectionError::VersionMismatch:
      m_dialog->OnConnectionError(
          _trans("The server and client's NetPlay versions are incompatible."));
      break;
    case ConnectionError::GameRunning:
      m_dialog->OnConnectionError(_trans("The game is currently running."));
      break;
    case ConnectionError::NameTooLong:
      m_dialog->OnConnectionError(_trans("Nickname is too long."));
      break;
    default:
      m_dialog->OnConnectionError(_trans("The server sent an unknown error message."));
      break;
    }

    Disconnect();
    return false;
  }
  else
  {
    rpac >> m_pid;

    Player player;
    player.name = m_player_name;
    player.pid = m_pid;
    player.revision = Common::GetNetplayDolphinVer();

    // add self to player list
    m_players[m_pid] = player;
    m_local_player = &m_players[m_pid];

    m_dialog->Update();

    m_is_connected = true;

    return true;
  }
}

static void ReceiveSyncIdentifier(sf::Packet& spac, SyncIdentifier& sync_identifier)
{
  // We use a temporary variable here due to a potential long vs long long mismatch
  sf::Uint64 dol_elf_size;
  spac >> dol_elf_size;
  sync_identifier.dol_elf_size = dol_elf_size;

  spac >> sync_identifier.game_id;
  spac >> sync_identifier.revision;
  spac >> sync_identifier.disc_number;
  spac >> sync_identifier.is_datel;

  for (u8& x : sync_identifier.sync_hash)
    spac >> x;
}

// called from ---NETPLAY--- thread
void NetPlayClient::OnData(sf::Packet& packet)
{
  MessageID mid;
  packet >> mid;

  INFO_LOG_FMT(NETPLAY, "Got server message: {:x}", static_cast<u8>(mid));

  switch (mid)
  {
  case MessageID::PlayerJoin:
    OnPlayerJoin(packet);
    break;

  case MessageID::PlayerLeave:
    OnPlayerLeave(packet);
    break;

  case MessageID::ChatMessage:
    OnChatMessage(packet);
    break;

  case MessageID::ChunkedDataStart:
    OnChunkedDataStart(packet);
    break;

  case MessageID::ChunkedDataEnd:
    OnChunkedDataEnd(packet);
    break;

  case MessageID::ChunkedDataPayload:
    OnChunkedDataPayload(packet);
    break;

  case MessageID::ChunkedDataAbort:
    OnChunkedDataAbort(packet);
    break;

  case MessageID::PadMapping:
    OnPadMapping(packet);
    break;

  case MessageID::GBAConfig:
    OnGBAConfig(packet);
    break;

  case MessageID::WiimoteMapping:
    OnWiimoteMapping(packet);
    break;

  case MessageID::PadData:
    OnPadData(packet);
    break;

  case MessageID::PadHostData:
    OnPadHostData(packet);
    break;

  case MessageID::WiimoteData:
    OnWiimoteData(packet);
    break;

  case MessageID::PadBuffer:
    OnPadBuffer(packet);
    break;

  case MessageID::HostInputAuthority:
    OnHostInputAuthority(packet);
    break;

  case MessageID::GolfSwitch:
    OnGolfSwitch(packet);
    break;

  case MessageID::GolfPrepare:
    OnGolfPrepare(packet);
    break;

  case MessageID::ChangeGame:
    OnChangeGame(packet);
    break;

  case MessageID::GameStatus:
    OnGameStatus(packet);
    break;

  case MessageID::StartGame:
    OnStartGame(packet);
    break;

  case MessageID::StopGame:
  case MessageID::DisableGame:
    OnStopGame(packet);
    break;

  case MessageID::PowerButton:
    OnPowerButton();
    break;

  case MessageID::Ping:
    OnPing(packet);
    break;

  case MessageID::PlayerPingData:
    OnPlayerPingData(packet);
    break;

  case MessageID::DesyncDetected:
    OnDesyncDetected(packet);
    break;

  case MessageID::SyncSaveData:
    OnSyncSaveData(packet);
    break;

  case MessageID::SyncCodes:
    OnSyncCodes(packet);
    break;

  case MessageID::ComputeGameDigest:
    OnComputeGameDigest(packet);
    break;

  case MessageID::GameDigestProgress:
    OnGameDigestProgress(packet);
    break;

  case MessageID::GameDigestResult:
    OnGameDigestResult(packet);
    break;

  case MessageID::GameDigestError:
    OnGameDigestError(packet);
    break;

  case MessageID::GameDigestAbort:
    OnGameDigestAbort();
    break;

  default:
    PanicAlertFmtT("Unknown message received with id : {0}", static_cast<u8>(mid));
    break;
  }
}

void NetPlayClient::OnPlayerJoin(sf::Packet& packet)
{
  Player player{};
  packet >> player.pid;
  packet >> player.name;
  packet >> player.revision;

  INFO_LOG_FMT(NETPLAY, "Player {} ({}) using {} joined", player.name, player.pid, player.revision);

  {
    std::lock_guard lkp(m_crit.players);
    m_players[player.pid] = player;
  }

  m_dialog->OnPlayerConnect(player.name);

  m_dialog->Update();
}

void NetPlayClient::OnPlayerLeave(sf::Packet& packet)
{
  PlayerId pid;
  packet >> pid;

  {
    std::lock_guard lkp(m_crit.players);
    const auto it = m_players.find(pid);
    if (it == m_players.end())
      return;

    const auto& player = it->second;
    INFO_LOG_FMT(NETPLAY, "Player {} ({}) left", player.name, pid);
    m_dialog->OnPlayerDisconnect(player.name);
    m_players.erase(m_players.find(pid));
  }

  m_dialog->Update();
}

void NetPlayClient::OnChatMessage(sf::Packet& packet)
{
  PlayerId pid;
  packet >> pid;
  std::string msg;
  packet >> msg;

  // don't need lock to read in this thread
  const Player& player = m_players[pid];

  INFO_LOG_FMT(NETPLAY, "Player {} ({}) wrote: {}", player.name, player.pid, msg);

  // add to gui
  m_dialog->AppendChat(fmt::format("{}[{}]: {}", player.name, pid, msg));
}

void NetPlayClient::OnChunkedDataStart(sf::Packet& packet)
{
  u32 cid;
  packet >> cid;
  std::string title;
  packet >> title;
  const u64 data_size = Common::PacketReadU64(packet);

  INFO_LOG_FMT(NETPLAY, "Starting data chunk {}.", cid);

  m_chunked_data_receive_queue.emplace(cid, sf::Packet{});

  std::vector<int> players;
  players.push_back(m_local_player->pid);
  m_dialog->ShowChunkedProgressDialog(title, data_size, players);
}

void NetPlayClient::OnChunkedDataEnd(sf::Packet& packet)
{
  u32 cid;
  packet >> cid;

  const auto data_packet_iter = m_chunked_data_receive_queue.find(cid);
  if (data_packet_iter == m_chunked_data_receive_queue.end())
  {
    INFO_LOG_FMT(NETPLAY, "Invalid data chunk ID {}.", cid);
    return;
  }

  INFO_LOG_FMT(NETPLAY, "Ending data chunk {}.", cid);

  auto& data_packet = data_packet_iter->second;
  OnData(data_packet);
  m_chunked_data_receive_queue.erase(data_packet_iter);
  m_dialog->HideChunkedProgressDialog();

  sf::Packet complete_packet;
  complete_packet << MessageID::ChunkedDataComplete;
  complete_packet << cid;
  Send(complete_packet, CHUNKED_DATA_CHANNEL);
}

void NetPlayClient::OnChunkedDataPayload(sf::Packet& packet)
{
  u32 cid;
  packet >> cid;

  const auto data_packet_iter = m_chunked_data_receive_queue.find(cid);
  if (data_packet_iter == m_chunked_data_receive_queue.end())
  {
    INFO_LOG_FMT(NETPLAY, "Invalid data chunk ID {}.", cid);
    return;
  }

  auto& data_packet = data_packet_iter->second;
  while (!packet.endOfPacket())
  {
    u8 byte;
    packet >> byte;
    data_packet << byte;
  }

  INFO_LOG_FMT(NETPLAY, "Received {} bytes of data chunk {}.", data_packet.getDataSize(), cid);

  m_dialog->SetChunkedProgress(m_local_player->pid, data_packet.getDataSize());

  sf::Packet progress_packet;
  progress_packet << MessageID::ChunkedDataProgress;
  progress_packet << cid;
  progress_packet << sf::Uint64{data_packet.getDataSize()};
  Send(progress_packet, CHUNKED_DATA_CHANNEL);
}

void NetPlayClient::OnChunkedDataAbort(sf::Packet& packet)
{
  u32 cid;
  packet >> cid;

  const auto iter = m_chunked_data_receive_queue.find(cid);
  if (iter == m_chunked_data_receive_queue.end())
  {
    INFO_LOG_FMT(NETPLAY, "Invalid data chunk ID {}.", cid);
    return;
  }

  INFO_LOG_FMT(NETPLAY, "Aborting data chunk {}.", cid);

  m_chunked_data_receive_queue.erase(iter);
  m_dialog->HideChunkedProgressDialog();
}

void NetPlayClient::OnPadMapping(sf::Packet& packet)
{
  for (PlayerId& mapping : m_pad_map)
    packet >> mapping;

  UpdateDevices();

  m_dialog->Update();
}

void NetPlayClient::OnWiimoteMapping(sf::Packet& packet)
{
  for (PlayerId& mapping : m_wiimote_map)
    packet >> mapping;

  m_dialog->Update();
}

void NetPlayClient::OnGBAConfig(sf::Packet& packet)
{
  for (size_t i = 0; i < m_gba_config.size(); ++i)
  {
    auto& config = m_gba_config[i];
    const auto old_config = config;

    packet >> config.enabled >> config.has_rom >> config.title;
    for (auto& data : config.hash)
      packet >> data;

    if (std::tie(config.has_rom, config.title, config.hash) !=
        std::tie(old_config.has_rom, old_config.title, old_config.hash))
    {
      m_dialog->OnMsgChangeGBARom(static_cast<int>(i), config);
      m_net_settings.gba_rom_paths[i] =
          config.has_rom ?
              m_dialog->FindGBARomPath(config.hash, config.title, static_cast<int>(i)) :
              "";
    }
  }

  SendGameStatus();
  UpdateDevices();

  m_dialog->Update();
}

void NetPlayClient::OnPadData(sf::Packet& packet)
{
  while (!packet.endOfPacket())
  {
    PadIndex map;
    packet >> map;

    GCPadStatus pad;
    packet >> pad.button;
    if (!m_gba_config.at(map).enabled)
    {
      packet >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >> pad.substickX >>
          pad.substickY >> pad.triggerLeft >> pad.triggerRight >> pad.isConnected;
    }

    // Trusting server for good map value (>=0 && <4)
    // add to pad buffer
    m_pad_buffer.at(map).Push(pad);
    m_gc_pad_event.Set();
  }
}

void NetPlayClient::OnPadHostData(sf::Packet& packet)
{
  while (!packet.endOfPacket())
  {
    PadIndex map;
    packet >> map;

    GCPadStatus pad;
    packet >> pad.button;
    if (!m_gba_config.at(map).enabled)
    {
      packet >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >> pad.substickX >>
          pad.substickY >> pad.triggerLeft >> pad.triggerRight >> pad.isConnected;
    }

    // Trusting server for good map value (>=0 && <4)
    // write to last status
    m_last_pad_status[map] = pad;

    if (!m_first_pad_status_received[map])
    {
      m_first_pad_status_received[map] = true;
      m_first_pad_status_received_event.Set();
    }
  }
}

void NetPlayClient::OnWiimoteData(sf::Packet& packet)
{
  while (!packet.endOfPacket())
  {
    PadIndex map;
    packet >> map;

    WiimoteEmu::SerializedWiimoteState pad;
    packet >> pad.length;
    ASSERT(pad.length <= pad.data.size());
    if (pad.length <= pad.data.size())
    {
      for (size_t i = 0; i < pad.length; ++i)
        packet >> pad.data[i];
    }
    else
    {
      pad.length = 0;
    }

    // Trusting server for good map value (>=0 && <4)
    // add to pad buffer
    m_wiimote_buffer.at(map).Push(pad);
    m_wii_pad_event.Set();
  }
}

void NetPlayClient::OnPadBuffer(sf::Packet& packet)
{
  u32 size = 0;
  packet >> size;

  m_target_buffer_size = size;
  m_dialog->OnPadBufferChanged(size);
}

void NetPlayClient::OnHostInputAuthority(sf::Packet& packet)
{
  packet >> m_host_input_authority;
  m_dialog->OnHostInputAuthorityChanged(m_host_input_authority);
}

void NetPlayClient::OnGolfSwitch(sf::Packet& packet)
{
  PlayerId pid;
  packet >> pid;

  const PlayerId previous_golfer = m_current_golfer;
  m_current_golfer = pid;
  m_dialog->OnGolferChanged(m_local_player->pid == pid, pid != 0 ? m_players[pid].name : "");

  if (m_local_player->pid == previous_golfer)
  {
    sf::Packet spac;
    spac << MessageID::GolfRelease;
    Send(spac);
  }
  else if (m_local_player->pid == pid)
  {
    sf::Packet spac;
    spac << MessageID::GolfAcquire;
    Send(spac);

    // Pads are already calibrated so we can just ignore this
    m_first_pad_status_received.fill(true);

    m_wait_on_input = false;
    m_wait_on_input_event.Set();
  }
}

void NetPlayClient::OnGolfPrepare(sf::Packet& packet)
{
  m_wait_on_input_received = true;
  m_wait_on_input = true;
}

void NetPlayClient::OnChangeGame(sf::Packet& packet)
{
  std::string netplay_name;
  {
    std::lock_guard lkg(m_crit.game);
    ReceiveSyncIdentifier(packet, m_selected_game);
    packet >> netplay_name;
  }

  INFO_LOG_FMT(NETPLAY, "Game changed to {}", netplay_name);

  // update gui
  m_dialog->OnMsgChangeGame(m_selected_game, netplay_name);

  SendGameStatus();

  sf::Packet client_capabilities_packet;
  client_capabilities_packet << MessageID::ClientCapabilities;
  client_capabilities_packet << ExpansionInterface::CEXIIPL::HasIPLDump();
  client_capabilities_packet << Config::Get(Config::SESSION_USE_FMA);
  Send(client_capabilities_packet);
}

void NetPlayClient::OnGameStatus(sf::Packet& packet)
{
  PlayerId pid;
  packet >> pid;

  {
    std::lock_guard lkp(m_crit.players);
    packet >> m_players[pid].game_status;
  }

  m_dialog->Update();
}

void NetPlayClient::OnStartGame(sf::Packet& packet)
{
  {
    std::lock_guard lkg(m_crit.game);

    INFO_LOG_FMT(NETPLAY, "Start of game {}", m_selected_game.game_id);

    packet >> m_current_game;
    packet >> m_net_settings.cpu_thread;
    packet >> m_net_settings.cpu_core;
    packet >> m_net_settings.enable_cheats;
    packet >> m_net_settings.selected_language;
    packet >> m_net_settings.override_region_settings;
    packet >> m_net_settings.dsp_enable_jit;
    packet >> m_net_settings.dsp_hle;
    packet >> m_net_settings.ram_override_enable;
    packet >> m_net_settings.mem1_size;
    packet >> m_net_settings.mem2_size;
    packet >> m_net_settings.fallback_region;
    packet >> m_net_settings.allow_sd_writes;
    packet >> m_net_settings.oc_enable;
    packet >> m_net_settings.oc_factor;

    for (auto slot : ExpansionInterface::SLOTS)
      packet >> m_net_settings.exi_device[slot];

    packet >> m_net_settings.memcard_size_override;

    for (u32& value : m_net_settings.sysconf_settings)
      packet >> value;

    packet >> m_net_settings.efb_access_enable;
    packet >> m_net_settings.bbox_enable;
    packet >> m_net_settings.force_progressive;
    packet >> m_net_settings.efb_to_texture_enable;
    packet >> m_net_settings.xfb_to_texture_enable;
    packet >> m_net_settings.disable_copy_to_vram;
    packet >> m_net_settings.immediate_xfb_enable;
    packet >> m_net_settings.efb_emulate_format_changes;
    packet >> m_net_settings.safe_texture_cache_color_samples;
    packet >> m_net_settings.perf_queries_enable;
    packet >> m_net_settings.float_exceptions;
    packet >> m_net_settings.divide_by_zero_exceptions;
    packet >> m_net_settings.alignment_exceptions;
    packet >> m_net_settings.fprf;
    packet >> m_net_settings.accurate_nans;
    packet >> m_net_settings.disable_icache;
    packet >> m_net_settings.sync_on_skip_idle;
    packet >> m_net_settings.sync_gpu;
    packet >> m_net_settings.sync_gpu_max_distance;
    packet >> m_net_settings.sync_gpu_min_distance;
    packet >> m_net_settings.sync_gpu_overclock;
    packet >> m_net_settings.jit_follow_branch;
    packet >> m_net_settings.fast_disc_speed;
    packet >> m_net_settings.mmu;
    packet >> m_net_settings.fastmem;
    packet >> m_net_settings.skip_ipl;
    packet >> m_net_settings.load_ipl_dump;
    packet >> m_net_settings.vertex_rounding;
    packet >> m_net_settings.internal_resolution;
    packet >> m_net_settings.efb_scaled_copy;
    packet >> m_net_settings.fast_depth_calc;
    packet >> m_net_settings.enable_pixel_lighting;
    packet >> m_net_settings.widescreen_hack;
    packet >> m_net_settings.force_texture_filtering;
    packet >> m_net_settings.max_anisotropy;
    packet >> m_net_settings.force_true_color;
    packet >> m_net_settings.disable_copy_filter;
    packet >> m_net_settings.disable_fog;
    packet >> m_net_settings.arbitrary_mipmap_detection;
    packet >> m_net_settings.arbitrary_mipmap_detection_threshold;
    packet >> m_net_settings.enable_gpu_texture_decoding;
    packet >> m_net_settings.defer_efb_copies;
    packet >> m_net_settings.efb_access_tile_size;
    packet >> m_net_settings.efb_access_defer_invalidation;
    packet >> m_net_settings.savedata_load;
    packet >> m_net_settings.savedata_write;
    packet >> m_net_settings.savedata_sync_all_wii;
    if (!m_net_settings.savedata_load)
    {
      m_net_settings.savedata_write = false;
      m_net_settings.savedata_sync_all_wii = false;
    }
    packet >> m_net_settings.strict_settings_sync;

    m_initial_rtc = Common::PacketReadU64(packet);

    packet >> m_net_settings.save_data_region;
    packet >> m_net_settings.sync_codes;

    packet >> m_net_settings.golf_mode;
    packet >> m_net_settings.use_fma;
    packet >> m_net_settings.hide_remote_gbas;

    for (size_t i = 0; i < sizeof(m_net_settings.sram); ++i)
      packet >> m_net_settings.sram[i];

    m_net_settings.is_hosting = m_local_player->IsHost();
  }

  m_dialog->OnMsgStartGame();
}

void NetPlayClient::OnStopGame(sf::Packet& packet)
{
  INFO_LOG_FMT(NETPLAY, "Game stopped");

  StopGame();
  m_dialog->OnMsgStopGame();
}

void NetPlayClient::OnPowerButton()
{
  InvokeStop();
  m_dialog->OnMsgPowerButton();
}

void NetPlayClient::OnPing(sf::Packet& packet)
{
  u32 ping_key = 0;
  packet >> ping_key;

  sf::Packet response_packet;
  response_packet << MessageID::Pong;
  response_packet << ping_key;

  Send(response_packet);
}

void NetPlayClient::OnPlayerPingData(sf::Packet& packet)
{
  PlayerId pid;
  packet >> pid;

  {
    std::lock_guard lkp(m_crit.players);
    Player& player = m_players[pid];
    packet >> player.ping;
  }

  DisplayPlayersPing();
  m_dialog->Update();
}

void NetPlayClient::OnDesyncDetected(sf::Packet& packet)
{
  int pid_to_blame;
  u32 frame;
  packet >> pid_to_blame;
  packet >> frame;

  std::string player = "??";
  std::lock_guard lkp(m_crit.players);
  {
    const auto it = m_players.find(pid_to_blame);
    if (it != m_players.end())
      player = it->second.name;
  }

  INFO_LOG_FMT(NETPLAY, "Player {} ({}) desynced!", player, pid_to_blame);

  m_dialog->OnDesync(frame, player);
}

void NetPlayClient::OnSyncSaveData(sf::Packet& packet)
{
  SyncSaveDataID sub_id;
  packet >> sub_id;

  if (m_local_player->IsHost())
    return;

  INFO_LOG_FMT(NETPLAY, "Processing OnSyncSaveData sub id: {}", static_cast<u8>(sub_id));

  switch (sub_id)
  {
  case SyncSaveDataID::Notify:
    OnSyncSaveDataNotify(packet);
    break;

  case SyncSaveDataID::RawData:
    OnSyncSaveDataRaw(packet);
    break;

  case SyncSaveDataID::GCIData:
    OnSyncSaveDataGCI(packet);
    break;

  case SyncSaveDataID::WiiData:
    OnSyncSaveDataWii(packet);
    break;

  case SyncSaveDataID::GBAData:
    OnSyncSaveDataGBA(packet);
    break;

  default:
    PanicAlertFmtT("Unknown SYNC_SAVE_DATA message received with id: {0}", static_cast<u8>(sub_id));
    break;
  }
}

void NetPlayClient::OnSyncSaveDataNotify(sf::Packet& packet)
{
  packet >> m_sync_save_data_count;
  m_sync_save_data_success_count = 0;

  INFO_LOG_FMT(NETPLAY, "Initializing wait for {} savegame chunks.", m_sync_save_data_count);

  if (m_sync_save_data_count == 0)
    SyncSaveDataResponse(true);
  else
    m_dialog->AppendChat(Common::GetStringT("Synchronizing save data..."));
}

void NetPlayClient::OnSyncSaveDataRaw(sf::Packet& packet)
{
  bool is_slot_a;
  std::string region;
  int size_override;
  packet >> is_slot_a >> region >> size_override;

  INFO_LOG_FMT(NETPLAY, "Received raw memcard data for slot {}: region {}, size override {}.",
               is_slot_a ? 'A' : 'B', region, size_override);

  // This check is mainly intended to filter out characters which have special meanings in paths
  if (region != JAP_DIR && region != USA_DIR && region != EUR_DIR)
  {
    WARN_LOG_FMT(NETPLAY, "Received invalid raw memory card region.");
    SyncSaveDataResponse(false);
    return;
  }

  std::string size_suffix;
  if (size_override >= 0 && size_override <= 4)
  {
    size_suffix = fmt::format(
        ".{}", Memcard::MbitToFreeBlocks(Memcard::MBIT_SIZE_MEMORY_CARD_59 << size_override));
  }

  const std::string path = File::GetUserPath(D_GCUSER_IDX) + GC_MEMCARD_NETPLAY +
                           (is_slot_a ? "A." : "B.") + region + size_suffix + ".raw";
  if (File::Exists(path) && !File::Delete(path))
  {
    PanicAlertFmtT("Failed to delete NetPlay memory card. Verify your write permissions.");
    SyncSaveDataResponse(false);
    return;
  }

  const bool success = DecompressPacketIntoFile(packet, path);
  SyncSaveDataResponse(success);
}

void NetPlayClient::OnSyncSaveDataGCI(sf::Packet& packet)
{
  bool is_slot_a;
  u8 file_count;
  packet >> is_slot_a >> file_count;

  const std::string path = File::GetUserPath(D_GCUSER_IDX) + GC_MEMCARD_NETPLAY DIR_SEP +
                           fmt::format("Card {}", is_slot_a ? 'A' : 'B');

  INFO_LOG_FMT(NETPLAY, "Received GCI memcard data for slot {}: {}, {} files.",
               is_slot_a ? 'A' : 'B', path, file_count);

  if ((File::Exists(path) && !File::DeleteDirRecursively(path + DIR_SEP)) ||
      !File::CreateFullPath(path + DIR_SEP))
  {
    PanicAlertFmtT("Failed to reset NetPlay GCI folder. Verify your write permissions.");
    SyncSaveDataResponse(false);
    return;
  }

  for (u8 i = 0; i < file_count; i++)
  {
    std::string file_name;
    packet >> file_name;

    INFO_LOG_FMT(NETPLAY, "Received GCI: {}", file_name);

    if (!Common::IsFileNameSafe(file_name) ||
        !DecompressPacketIntoFile(packet, path + DIR_SEP + file_name))
    {
      WARN_LOG_FMT(NETPLAY, "Received invalid GCI.");
      SyncSaveDataResponse(false);
      return;
    }
  }

  SyncSaveDataResponse(true);
}

void NetPlayClient::OnSyncSaveDataWii(sf::Packet& packet)
{
  const std::string path = File::GetUserPath(D_USER_IDX) + "Wii" GC_MEMCARD_NETPLAY DIR_SEP;
  std::string redirect_path = File::GetUserPath(D_USER_IDX) + "Redirect" GC_MEMCARD_NETPLAY DIR_SEP;

  if (File::Exists(path) && !File::DeleteDirRecursively(path))
  {
    PanicAlertFmtT("Failed to reset NetPlay NAND folder. Verify your write permissions.");
    SyncSaveDataResponse(false);
    return;
  }
  if (File::Exists(redirect_path) && !File::DeleteDirRecursively(redirect_path))
  {
    PanicAlertFmtT("Failed to reset NetPlay redirect folder. Verify your write permissions.");
    SyncSaveDataResponse(false);
    return;
  }

  auto temp_fs = std::make_unique<IOS::HLE::FS::HostFileSystem>(path);
  std::vector<u64> titles;

  constexpr IOS::HLE::FS::Modes fs_modes{
      IOS::HLE::FS::Mode::ReadWrite,
      IOS::HLE::FS::Mode::ReadWrite,
      IOS::HLE::FS::Mode::ReadWrite,
  };

  // Read the Mii data
  bool mii_data;
  packet >> mii_data;
  if (mii_data)
  {
    INFO_LOG_FMT(NETPLAY, "Received Mii data.");

    auto buffer = DecompressPacketIntoBuffer(packet);

    temp_fs->CreateFullPath(IOS::PID_KERNEL, IOS::PID_KERNEL, "/shared2/menu/FaceLib/", 0,
                            fs_modes);
    auto file = temp_fs->CreateAndOpenFile(IOS::PID_KERNEL, IOS::PID_KERNEL,
                                           Common::GetMiiDatabasePath(), fs_modes);

    if (!buffer || !file || !file->Write(buffer->data(), buffer->size()))
    {
      PanicAlertFmtT("Failed to write Mii data.");
      SyncSaveDataResponse(false);
      return;
    }
  }

  // Read the saves
  u32 save_count;
  packet >> save_count;
  INFO_LOG_FMT(NETPLAY, "Received data for {} Wii saves.", save_count);
  for (u32 n = 0; n < save_count; n++)
  {
    u64 title_id = Common::PacketReadU64(packet);
    titles.push_back(title_id);
    temp_fs->CreateFullPath(IOS::PID_KERNEL, IOS::PID_KERNEL,
                            Common::GetTitleDataPath(title_id) + '/', 0, fs_modes);
    auto save = WiiSave::MakeNandStorage(temp_fs.get(), title_id);

    bool exists;
    packet >> exists;
    if (!exists)
    {
      INFO_LOG_FMT(NETPLAY, "No data for Wii save of title {:016x}.", title_id);
      continue;
    }

    INFO_LOG_FMT(NETPLAY, "Received Wii save of title {:016x}.", title_id);

    // Header
    WiiSave::Header header;
    packet >> header.tid;
    packet >> header.banner_size;
    packet >> header.permissions;
    packet >> header.unk1;
    for (u8& byte : header.md5)
      packet >> byte;
    packet >> header.unk2;
    for (size_t i = 0; i < header.banner_size; i++)
      packet >> header.banner[i];

    // BkHeader
    WiiSave::BkHeader bk_header;
    packet >> bk_header.size;
    packet >> bk_header.magic;
    packet >> bk_header.ngid;
    packet >> bk_header.number_of_files;
    packet >> bk_header.size_of_files;
    packet >> bk_header.unk1;
    packet >> bk_header.unk2;
    packet >> bk_header.total_size;
    for (u8& byte : bk_header.unk3)
      packet >> byte;
    packet >> bk_header.tid;
    for (u8& byte : bk_header.mac_address)
      packet >> byte;

    // Files
    std::vector<WiiSave::Storage::SaveFile> files;
    for (u32 i = 0; i < bk_header.number_of_files; i++)
    {
      WiiSave::Storage::SaveFile file;
      packet >> file.mode >> file.attributes;
      packet >> file.type;
      packet >> file.path;

      INFO_LOG_FMT(NETPLAY, "Received Wii save data of type {} at {}", static_cast<u8>(file.type),
                   file.path);

      if (file.type == WiiSave::Storage::SaveFile::Type::File)
      {
        auto buffer = DecompressPacketIntoBuffer(packet);
        if (!buffer)
        {
          SyncSaveDataResponse(false);
          return;
        }

        file.data = std::move(*buffer);
      }

      files.push_back(std::move(file));
    }

    if (!save->WriteHeader(header) || !save->WriteBkHeader(bk_header) || !save->WriteFiles(files))
    {
      PanicAlertFmtT("Failed to write Wii save.");
      SyncSaveDataResponse(false);
      return;
    }
  }

  bool has_redirected_save;
  packet >> has_redirected_save;
  if (has_redirected_save)
  {
    INFO_LOG_FMT(NETPLAY, "Received redirected save.");
    if (!DecompressPacketIntoFolder(packet, redirect_path))
    {
      PanicAlertFmtT("Failed to write redirected save.");
      SyncSaveDataResponse(false);
      return;
    }
  }

  SetWiiSyncData(std::move(temp_fs), std::move(titles), std::move(redirect_path));
  SyncSaveDataResponse(true);
}

void NetPlayClient::OnSyncSaveDataGBA(sf::Packet& packet)
{
  u8 slot;
  packet >> slot;

  INFO_LOG_FMT(NETPLAY, "Received GBA save for slot {}.", slot);

  const std::string path =
      fmt::format("{}{}{}.sav", File::GetUserPath(D_GBAUSER_IDX), GBA_SAVE_NETPLAY, slot + 1);
  if (File::Exists(path) && !File::Delete(path))
  {
    PanicAlertFmtT("Failed to delete NetPlay GBA{0} save file. Verify your write permissions.",
                   slot + 1);
    SyncSaveDataResponse(false);
    return;
  }

  const bool success = DecompressPacketIntoFile(packet, path);
  SyncSaveDataResponse(success);
}

void NetPlayClient::OnSyncCodes(sf::Packet& packet)
{
  // Recieve Data Packet
  SyncCodeID sub_id;
  packet >> sub_id;

  INFO_LOG_FMT(NETPLAY, "Processing OnSyncCodes sub id: {}", static_cast<u8>(sub_id));

  // Check Which Operation to Perform with This Packet
  switch (sub_id)
  {
  case SyncCodeID::Notify:
    OnSyncCodesNotify();
    break;

  case SyncCodeID::NotifyGecko:
    OnSyncCodesNotifyGecko(packet);
    break;

  case SyncCodeID::GeckoData:
    OnSyncCodesDataGecko(packet);
    break;

  case SyncCodeID::NotifyAR:
    OnSyncCodesNotifyAR(packet);
    break;

  case SyncCodeID::ARData:
    OnSyncCodesDataAR(packet);
    break;

  default:
    PanicAlertFmtT("Unknown SYNC_CODES message received with id: {0}", static_cast<u8>(sub_id));
    break;
  }
}

void NetPlayClient::OnSyncCodesNotify()
{
  // Set both codes as unsynced
  m_sync_gecko_codes_complete = false;
  m_sync_ar_codes_complete = false;
}

void NetPlayClient::OnSyncCodesNotifyGecko(sf::Packet& packet)
{
  // Return if this is the host
  if (m_local_player->IsHost())
    return;

  // Receive Number of Codelines to Receive
  packet >> m_sync_gecko_codes_count;

  m_sync_gecko_codes_success_count = 0;

  INFO_LOG_FMT(NETPLAY, "Receiving {} Gecko codelines", m_sync_gecko_codes_count);

  // Check if no codes to sync, if so return as finished
  if (m_sync_gecko_codes_count == 0)
  {
    m_sync_gecko_codes_complete = true;
    SyncCodeResponse(true);
  }
  else
  {
    m_dialog->AppendChat(Common::GetStringT("Synchronizing Gecko codes..."));
  }
}

void NetPlayClient::OnSyncCodesDataGecko(sf::Packet& packet)
{
  // Return if this is the host
  if (m_local_player->IsHost())
    return;

  std::vector<Gecko::GeckoCode> synced_codes;
  synced_codes.reserve(m_sync_gecko_codes_count);

  Gecko::GeckoCode gcode{};
  gcode.name = "Synced Codes";
  gcode.enabled = true;

  // Receive code contents from packet
  for (u32 i = 0; i < m_sync_gecko_codes_count; i++)
  {
    Gecko::GeckoCode::Code new_code;
    packet >> new_code.address;
    packet >> new_code.data;

    INFO_LOG_FMT(NETPLAY, "Received {:08x} {:08x}", new_code.address, new_code.data);

    gcode.codes.push_back(std::move(new_code));

    if (++m_sync_gecko_codes_success_count >= m_sync_gecko_codes_count)
    {
      m_sync_gecko_codes_complete = true;
      SyncCodeResponse(true);
    }
  }

  // Add gcode containing all codes to Gecko Code vector
  synced_codes.push_back(std::move(gcode));

  // Clear Vector if received 0 codes (match host's end when using no codes)
  if (m_sync_gecko_codes_count == 0)
    synced_codes.clear();

  // Copy this to the vector located in GeckoCode.cpp
  Gecko::UpdateSyncedCodes(synced_codes);
}

void NetPlayClient::OnSyncCodesNotifyAR(sf::Packet& packet)
{
  // Return if this is the host
  if (m_local_player->IsHost())
    return;

  // Receive Number of Codelines to Receive
  packet >> m_sync_ar_codes_count;

  m_sync_ar_codes_success_count = 0;

  INFO_LOG_FMT(NETPLAY, "Receiving {} AR codelines", m_sync_ar_codes_count);

  // Check if no codes to sync, if so return as finished
  if (m_sync_ar_codes_count == 0)
  {
    m_sync_ar_codes_complete = true;
    SyncCodeResponse(true);
  }
  else
  {
    m_dialog->AppendChat(Common::GetStringT("Synchronizing AR codes..."));
  }
}

void NetPlayClient::OnSyncCodesDataAR(sf::Packet& packet)
{
  // Return if this is the host
  if (m_local_player->IsHost())
    return;

  std::vector<ActionReplay::ARCode> synced_codes;
  synced_codes.reserve(m_sync_ar_codes_count);

  ActionReplay::ARCode arcode{};
  arcode.name = "Synced Codes";
  arcode.enabled = true;

  // Receive code contents from packet
  for (u32 i = 0; i < m_sync_ar_codes_count; i++)
  {
    ActionReplay::AREntry new_code;
    packet >> new_code.cmd_addr;
    packet >> new_code.value;

    INFO_LOG_FMT(NETPLAY, "Received {:08x} {:08x}", new_code.cmd_addr, new_code.value);
    arcode.ops.push_back(new_code);

    if (++m_sync_ar_codes_success_count >= m_sync_ar_codes_count)
    {
      m_sync_ar_codes_complete = true;
      SyncCodeResponse(true);
    }
  }

  // Add arcode containing all codes to AR Code vector
  synced_codes.push_back(std::move(arcode));

  // Clear Vector if received 0 codes (match host's end when using no codes)
  if (m_sync_ar_codes_count == 0)
    synced_codes.clear();

  // Copy this to the vector located in ActionReplay.cpp
  ActionReplay::UpdateSyncedCodes(synced_codes);
}

void NetPlayClient::OnComputeGameDigest(sf::Packet& packet)
{
  SyncIdentifier sync_identifier;
  ReceiveSyncIdentifier(packet, sync_identifier);

  ComputeGameDigest(sync_identifier);
}

void NetPlayClient::OnGameDigestProgress(sf::Packet& packet)
{
  PlayerId pid;
  int progress;
  packet >> pid;
  packet >> progress;

  m_dialog->SetGameDigestProgress(pid, progress);
}

void NetPlayClient::OnGameDigestResult(sf::Packet& packet)
{
  PlayerId pid;
  std::string result;
  packet >> pid;
  packet >> result;

  m_dialog->SetGameDigestResult(pid, result);
}

void NetPlayClient::OnGameDigestError(sf::Packet& packet)
{
  PlayerId pid;
  std::string error;
  packet >> pid;
  packet >> error;

  m_dialog->SetGameDigestResult(pid, error);
}

void NetPlayClient::OnGameDigestAbort()
{
  m_should_compute_game_digest = false;
  m_dialog->AbortGameDigest();
}

void NetPlayClient::Send(const sf::Packet& packet, const u8 channel_id)
{
  Common::ENet::SendPacket(m_server, packet, channel_id);
}

void NetPlayClient::DisplayPlayersPing()
{
  if (!g_ActiveConfig.bShowNetPlayPing)
    return;

  OSD::AddTypedMessage(OSD::MessageType::NetPlayPing, fmt::format("Ping: {}", GetPlayersMaxPing()),
                       OSD::Duration::SHORT, OSD::Color::CYAN);
}

u32 NetPlayClient::GetPlayersMaxPing() const
{
  return std::max_element(
             m_players.begin(), m_players.end(),
             [](const auto& a, const auto& b) { return a.second.ping < b.second.ping; })
      ->second.ping;
}

void NetPlayClient::Disconnect()
{
  ENetEvent netEvent;
  m_connecting = false;
  m_connection_state = ConnectionState::Failure;
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

void NetPlayClient::SendAsync(sf::Packet&& packet, const u8 channel_id)
{
  {
    std::lock_guard lkq(m_crit.async_queue_write);
    m_async_queue.Push(AsyncQueueEntry{std::move(packet), channel_id});
  }
  Common::ENet::WakeupThread(m_client);
}

// called from ---NETPLAY--- thread
void NetPlayClient::ThreadFunc()
{
  INFO_LOG_FMT(NETPLAY, "NetPlayClient starting.");

  Common::QoSSession qos_session;
  if (Config::Get(Config::NETPLAY_ENABLE_QOS))
  {
    qos_session = Common::QoSSession(m_server);

    if (qos_session.Successful())
    {
      m_dialog->AppendChat(
          Common::GetStringT("Quality of Service (QoS) was successfully enabled."));
    }
    else
    {
      m_dialog->AppendChat(Common::GetStringT("Quality of Service (QoS) couldn't be enabled."));
    }
  }

  while (m_do_loop.IsSet())
  {
    ENetEvent netEvent;
    int net;
    if (m_traversal_client)
      m_traversal_client->HandleResends();
    net = enet_host_service(m_client, &netEvent, 250);
    while (!m_async_queue.Empty())
    {
      INFO_LOG_FMT(NETPLAY, "Processing async queue event.");
      {
        auto& e = m_async_queue.Front();
        Send(e.packet, e.channel_id);
      }
      INFO_LOG_FMT(NETPLAY, "Processing async queue event done.");
      m_async_queue.Pop();
    }
    if (net > 0)
    {
      sf::Packet rpac;
      switch (netEvent.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        INFO_LOG_FMT(NETPLAY, "enet_host_service: connect event");
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        INFO_LOG_FMT(NETPLAY, "enet_host_service: receive event");

        rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
        OnData(rpac);

        enet_packet_destroy(netEvent.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        INFO_LOG_FMT(NETPLAY, "enet_host_service: disconnect event");

        m_dialog->OnConnectionLost();

        if (m_is_running.IsSet())
          StopGame();

        break;
      default:
        // not a valid switch case due to not technically being part of the enum
        if (static_cast<int>(netEvent.type) == Common::ENet::SKIPPABLE_EVENT)
          INFO_LOG_FMT(NETPLAY, "enet_host_service: skippable packet event");
        else
          ERROR_LOG_FMT(NETPLAY, "enet_host_service: unknown event type: {}", int(netEvent.type));
        break;
      }
    }
    else if (net == 0)
    {
      INFO_LOG_FMT(NETPLAY, "enet_host_service: no event occurred");
    }
    else
    {
      ERROR_LOG_FMT(NETPLAY, "enet_host_service error: {}", net);
    }
  }

  INFO_LOG_FMT(NETPLAY, "NetPlayClient shutting down.");

  Disconnect();
  return;
}

// called from ---GUI--- thread
std::vector<const Player*> NetPlayClient::GetPlayers()
{
  std::lock_guard lkp(m_crit.players);
  std::vector<const Player*> players;

  for (const auto& pair : m_players)
    players.push_back(&pair.second);

  return players;
}

const NetSettings& NetPlayClient::GetNetSettings() const
{
  return m_net_settings;
}

// called from ---GUI--- thread
void NetPlayClient::SendChatMessage(const std::string& msg)
{
  sf::Packet packet;
  packet << MessageID::ChatMessage;
  packet << msg;

  SendAsync(std::move(packet));
}

// called from ---CPU--- thread
void NetPlayClient::AddPadStateToPacket(const int in_game_pad, const GCPadStatus& pad,
                                        sf::Packet& packet)
{
  packet << static_cast<PadIndex>(in_game_pad);
  packet << pad.button;
  if (!m_gba_config[in_game_pad].enabled)
  {
    packet << pad.analogA << pad.analogB << pad.stickX << pad.stickY << pad.substickX
           << pad.substickY << pad.triggerLeft << pad.triggerRight << pad.isConnected;
  }
}

// called from ---CPU--- thread
void NetPlayClient::AddWiimoteStateToPacket(int in_game_pad,
                                            const WiimoteEmu::SerializedWiimoteState& state,
                                            sf::Packet& packet)
{
  packet << static_cast<PadIndex>(in_game_pad);
  packet << state.length;
  for (size_t i = 0; i < state.length; ++i)
    packet << state.data[i];
}

// called from ---GUI--- thread
void NetPlayClient::SendStartGamePacket()
{
  sf::Packet packet;
  packet << MessageID::StartGame;
  packet << m_current_game;

  SendAsync(std::move(packet));
}

// called from ---GUI--- thread
void NetPlayClient::SendStopGamePacket()
{
  sf::Packet packet;
  packet << MessageID::StopGame;

  SendAsync(std::move(packet));
}

// called from ---GUI--- thread
bool NetPlayClient::StartGame(const std::string& path)
{
  std::lock_guard lkg(m_crit.game);
  SendStartGamePacket();

  if (m_is_running.IsSet())
  {
    PanicAlertFmtT("Game is already running!");
    return false;
  }

  m_timebase_frame = 0;
  m_current_golfer = 1;
  m_wait_on_input = false;

  m_is_running.Set();
  NetPlay_Enable(this);

  ClearBuffers();

  m_first_pad_status_received.fill(false);

  if (m_dialog->IsRecording())
  {
    auto& movie = Core::System::GetInstance().GetMovie();
    if (movie.IsReadOnly())
      movie.SetReadOnly(false);

    Movie::ControllerTypeArray controllers{};
    Movie::WiimoteEnabledArray wiimotes{};
    for (unsigned int i = 0; i < 4; ++i)
    {
      if (m_pad_map[i] > 0 && m_gba_config[i].enabled)
        controllers[i] = Movie::ControllerType::GBA;
      else if (m_pad_map[i] > 0)
        controllers[i] = Movie::ControllerType::GC;
      else
        controllers[i] = Movie::ControllerType::None;
      wiimotes[i] = m_wiimote_map[i] > 0;
    }
    movie.BeginRecordingInput(controllers, wiimotes);
  }

  for (unsigned int i = 0; i < 4; ++i)
  {
    Config::SetCurrent(Config::GetInfoForWiimoteSource(i),
                       m_wiimote_map[i] > 0 ? WiimoteSource::Emulated : WiimoteSource::None);
  }

  // boot game
  auto boot_session_data = std::make_unique<BootSessionData>();

  INFO_LOG_FMT(NETPLAY,
               "Setting Wii sync data: has FS {}, sync_titles = {:016x}, redirect folder = {}",
               !!m_wii_sync_fs, fmt::join(m_wii_sync_titles, ", "), m_wii_sync_redirect_folder);

  boot_session_data->SetWiiSyncData(std::move(m_wii_sync_fs), std::move(m_wii_sync_titles),
                                    std::move(m_wii_sync_redirect_folder), [] {
                                      // on emulation end clean up the Wii save sync directory --
                                      // see OnSyncSaveDataWii()
                                      const std::string wii_path = File::GetUserPath(D_USER_IDX) +
                                                                   "Wii" GC_MEMCARD_NETPLAY DIR_SEP;
                                      if (File::Exists(wii_path))
                                        File::DeleteDirRecursively(wii_path);
                                      const std::string redirect_path =
                                          File::GetUserPath(D_USER_IDX) +
                                          "Redirect" GC_MEMCARD_NETPLAY DIR_SEP;
                                      if (File::Exists(redirect_path))
                                        File::DeleteDirRecursively(redirect_path);
                                    });
  boot_session_data->SetNetplaySettings(std::make_unique<NetPlay::NetSettings>(m_net_settings));

  m_dialog->BootGame(path, std::move(boot_session_data));

  UpdateDevices();

  return true;
}

void NetPlayClient::SyncSaveDataResponse(const bool success)
{
  m_dialog->AppendChat(success ? Common::GetStringT("Data received!") :
                                 Common::GetStringT("Error processing data."));

  if (success)
  {
    if (++m_sync_save_data_success_count >= m_sync_save_data_count)
    {
      sf::Packet response_packet;
      response_packet << MessageID::SyncSaveData;
      response_packet << SyncSaveDataID::Success;

      Send(response_packet);
    }
  }
  else
  {
    sf::Packet response_packet;
    response_packet << MessageID::SyncSaveData;
    response_packet << SyncSaveDataID::Failure;

    Send(response_packet);
  }
}

void NetPlayClient::SyncCodeResponse(const bool success)
{
  // If something failed, immediately report back that code sync failed
  if (!success)
  {
    m_dialog->AppendChat(Common::GetStringT("Error processing codes."));

    sf::Packet response_packet;
    response_packet << MessageID::SyncCodes;
    response_packet << SyncCodeID::Failure;

    Send(response_packet);
    return;
  }

  // If both gecko and AR codes have completely finished transferring, report back as successful
  if (m_sync_gecko_codes_complete && m_sync_ar_codes_complete)
  {
    m_dialog->AppendChat(Common::GetStringT("Codes received!"));

    sf::Packet response_packet;
    response_packet << MessageID::SyncCodes;
    response_packet << SyncCodeID::Success;

    Send(response_packet);
  }
}

// called from ---GUI--- thread
bool NetPlayClient::ChangeGame(const std::string&)
{
  return true;
}

// called from ---NETPLAY--- thread
void NetPlayClient::UpdateDevices()
{
  u8 local_pad = 0;
  u8 pad = 0;

  auto& si = Core::System::GetInstance().GetSerialInterface();
  for (auto player_id : m_pad_map)
  {
    if (m_gba_config[pad].enabled && player_id > 0)
    {
      si.ChangeDevice(SerialInterface::SIDEVICE_GC_GBA_EMULATED, pad);
    }
    else if (player_id == m_local_player->pid)
    {
      // Use local controller types for local controllers if they are compatible
      const SerialInterface::SIDevices si_device =
          Config::Get(Config::GetInfoForSIDevice(local_pad));
      if (SerialInterface::SIDevice_IsGCController(si_device))
      {
        si.ChangeDevice(si_device, pad);

        if (si_device == SerialInterface::SIDEVICE_WIIU_ADAPTER)
        {
          GCAdapter::ResetDeviceType(local_pad);
        }
      }
      else
      {
        si.ChangeDevice(SerialInterface::SIDEVICE_GC_CONTROLLER, pad);
      }
      local_pad++;
    }
    else if (player_id > 0)
    {
      si.ChangeDevice(SerialInterface::SIDEVICE_GC_CONTROLLER, pad);
    }
    else
    {
      si.ChangeDevice(SerialInterface::SIDEVICE_NONE, pad);
    }
    pad++;
  }
}

// called from ---NETPLAY--- thread
void NetPlayClient::ClearBuffers()
{
  // clear pad buffers, Clear method isn't thread safe
  for (unsigned int i = 0; i < 4; ++i)
  {
    while (m_pad_buffer[i].Size())
      m_pad_buffer[i].Pop();

    while (m_wiimote_buffer[i].Size())
      m_wiimote_buffer[i].Pop();
  }
}

// called from ---NETPLAY--- thread
void NetPlayClient::OnTraversalStateChanged()
{
  const Common::TraversalClient::State state = m_traversal_client->GetState();

  if (m_connection_state == ConnectionState::WaitingForTraversalClientConnection &&
      state == Common::TraversalClient::State::Connected)
  {
    m_connection_state = ConnectionState::WaitingForTraversalClientConnectReady;
    m_traversal_client->ConnectToClient(m_host_spec);
  }
  else if (m_connection_state != ConnectionState::Failure &&
           state == Common::TraversalClient::State::Failure)
  {
    Disconnect();
    m_dialog->OnTraversalError(m_traversal_client->GetFailureReason());
  }
  m_dialog->OnTraversalStateChanged(state);
}

// called from ---NETPLAY--- thread
void NetPlayClient::OnConnectReady(ENetAddress addr)
{
  if (m_connection_state == ConnectionState::WaitingForTraversalClientConnectReady)
  {
    m_connection_state = ConnectionState::Connecting;
    enet_host_connect(m_client, &addr, CHANNEL_COUNT, 0);
  }
}

// called from ---NETPLAY--- thread
void NetPlayClient::OnConnectFailed(Common::TraversalConnectFailedReason reason)
{
  m_connecting = false;
  m_connection_state = ConnectionState::Failure;
  switch (reason)
  {
  case Common::TraversalConnectFailedReason::ClientDidntRespond:
    PanicAlertFmtT("Traversal server timed out connecting to the host");
    break;
  case Common::TraversalConnectFailedReason::ClientFailure:
    PanicAlertFmtT("Server rejected traversal attempt");
    break;
  case Common::TraversalConnectFailedReason::NoSuchClient:
    PanicAlertFmtT("Invalid host");
    break;
  default:
    PanicAlertFmtT("Unknown error {0:x}", static_cast<int>(reason));
    break;
  }
}

// called from ---CPU--- thread
bool NetPlayClient::GetNetPads(const int pad_nb, const bool batching, GCPadStatus* pad_status)
{
  // The interface for this is extremely silly.
  //
  // Imagine a physical device that links three GameCubes together
  // and emulates NetPlay that way. Which GameCube controls which
  // in-game controllers can be configured on the device (m_pad_map)
  // but which sockets on each individual GameCube should be used
  // to control which players? The solution that Dolphin uses is
  // that we hardcode the knowledge that they go in order, so if
  // you have a 3P game with three GameCubes, then every single
  // controller should be plugged into slot 1.
  //
  // If you have a 4P game, then one of the GameCubes will have
  // a controller plugged into slot 1, and another in slot 2.
  //
  // The slot number is the "local" pad number, and what player
  // it actually means is the "in-game" pad number.

  // When the 1st in-game pad is polled and batching is set, the
  // others will be polled as well. To reduce latency, we poll all
  // local controllers at once and then send the status to the other
  // clients.
  //
  // Batching is enabled when polled from VI. If batching is not
  // enabled, the poll is probably from MMIO, which can poll any
  // specific pad arbitrarily. In this case, we poll just that pad
  // and send it.

  // When here when told to so we don't deadlock in certain situations
  while (m_wait_on_input)
  {
    if (!m_is_running.IsSet())
    {
      return false;
    }

    if (m_wait_on_input_received)
    {
      // Tell the server we've acknowledged the message
      sf::Packet spac;
      spac << MessageID::GolfPrepare;
      Send(spac);

      m_wait_on_input_received = false;
    }

    m_wait_on_input_event.Wait();
  }

  if (IsFirstInGamePad(pad_nb) && batching)
  {
    sf::Packet packet;
    packet << MessageID::PadData;

    bool send_packet = false;
    const int num_local_pads = NumLocalPads();
    for (int local_pad = 0; local_pad < num_local_pads; local_pad++)
    {
      send_packet = PollLocalPad(local_pad, packet) || send_packet;
    }

    if (send_packet)
      SendAsync(std::move(packet));

    if (m_host_input_authority)
      SendPadHostPoll(-1);
  }

  if (!batching)
  {
    const int local_pad = InGamePadToLocalPad(pad_nb);
    if (local_pad < 4)
    {
      sf::Packet packet;
      packet << MessageID::PadData;
      if (PollLocalPad(local_pad, packet))
        SendAsync(std::move(packet));
    }

    if (m_host_input_authority)
      SendPadHostPoll(pad_nb);
  }

  if (m_host_input_authority)
  {
    if (m_local_player->pid != m_current_golfer)
    {
      // CoreTiming acts funny and causes what looks like frame skip if
      // we toggle the emulation speed too quickly, so to prevent this
      // we wait until the buffer has been over for at least 1 second.

      const bool buffer_over_target = m_pad_buffer[pad_nb].Size() > m_target_buffer_size + 1;
      if (!buffer_over_target)
        m_buffer_under_target_last = std::chrono::steady_clock::now();

      std::chrono::duration<double> time_diff =
          std::chrono::steady_clock::now() - m_buffer_under_target_last;
      if (time_diff.count() >= 1.0 || !buffer_over_target)
      {
        // run fast if the buffer is overfilled, otherwise run normal speed
        Config::SetCurrent(Config::MAIN_EMULATION_SPEED, buffer_over_target ? 0.0f : 1.0f);
      }
    }
    else
    {
      // Set normal speed when we're the host, otherwise it can get stuck at unlimited
      Config::SetCurrent(Config::MAIN_EMULATION_SPEED, 1.0f);
    }
  }

  // Now, we either use the data pushed earlier, or wait for the
  // other clients to send it to us
  while (m_pad_buffer[pad_nb].Size() == 0)
  {
    if (!m_is_running.IsSet())
    {
      return false;
    }

    m_gc_pad_event.Wait();
  }

  m_pad_buffer[pad_nb].Pop(*pad_status);

  auto& movie = Core::System::GetInstance().GetMovie();
  if (movie.IsRecordingInput())
  {
    movie.RecordInput(pad_status, pad_nb);
    movie.InputUpdate();
  }
  else
  {
    movie.CheckPadStatus(pad_status, pad_nb);
  }

  return true;
}

u64 NetPlayClient::GetInitialRTCValue() const
{
  return m_initial_rtc;
}

// called from ---CPU--- thread
bool NetPlayClient::WiimoteUpdate(const std::span<WiimoteDataBatchEntry>& entries)
{
  for (const WiimoteDataBatchEntry& entry : entries)
  {
    const int local_wiimote = InGameWiimoteToLocalWiimote(entry.wiimote);
    DEBUG_LOG_FMT(NETPLAY,
                  "Entering WiimoteUpdate() with wiimote {}, local_wiimote {}, state [{:02x}]",
                  entry.wiimote, local_wiimote,
                  fmt::join(std::span(entry.state->data.data(), entry.state->length), ", "));
    if (local_wiimote < 4)
    {
      sf::Packet packet;
      packet << MessageID::WiimoteData;
      if (AddLocalWiimoteToBuffer(local_wiimote, *entry.state, packet))
        SendAsync(std::move(packet));
    }

    // Now, we either use the data pushed earlier, or wait for the
    // other clients to send it to us
    while (m_wiimote_buffer[entry.wiimote].Size() == 0)
    {
      if (!m_is_running.IsSet())
      {
        return false;
      }

      m_wii_pad_event.Wait();
    }

    m_wiimote_buffer[entry.wiimote].Pop(*entry.state);

    DEBUG_LOG_FMT(NETPLAY, "Exiting WiimoteUpdate() with wiimote {}, state [{:02x}]", entry.wiimote,
                  fmt::join(std::span(entry.state->data.data(), entry.state->length), ", "));
  }

  return true;
}

bool NetPlayClient::PollLocalPad(const int local_pad, sf::Packet& packet)
{
  const int ingame_pad = LocalPadToInGamePad(local_pad);
  bool data_added = false;
  GCPadStatus pad_status;

  if (m_gba_config[ingame_pad].enabled)
  {
    pad_status = Pad::GetGBAStatus(local_pad);
  }
  else if (Config::Get(Config::GetInfoForSIDevice(local_pad)) ==
           SerialInterface::SIDEVICE_WIIU_ADAPTER)
  {
    pad_status = GCAdapter::Input(local_pad);
  }
  else
  {
    pad_status = Pad::GetStatus(local_pad);
  }

  if (m_host_input_authority)
  {
    if (m_local_player->pid != m_current_golfer)
    {
      // add to packet
      AddPadStateToPacket(ingame_pad, pad_status, packet);
      data_added = true;
    }
    else
    {
      // set locally
      m_last_pad_status[ingame_pad] = pad_status;
      m_first_pad_status_received[ingame_pad] = true;
    }
  }
  else
  {
    // adjust the buffer either up or down
    // inserting multiple padstates or dropping states
    while (m_pad_buffer[ingame_pad].Size() <= m_target_buffer_size)
    {
      // add to buffer
      m_pad_buffer[ingame_pad].Push(pad_status);

      // add to packet
      AddPadStateToPacket(ingame_pad, pad_status, packet);
      data_added = true;
    }
  }

  return data_added;
}

bool NetPlayClient::AddLocalWiimoteToBuffer(const int local_wiimote,
                                            const WiimoteEmu::SerializedWiimoteState& state,
                                            sf::Packet& packet)
{
  const int ingame_pad = LocalWiimoteToInGameWiimote(local_wiimote);
  bool data_added = false;

  // adjust the buffer either up or down
  // inserting multiple padstates or dropping states
  while (m_wiimote_buffer[ingame_pad].Size() <= m_target_buffer_size)
  {
    // add to buffer
    m_wiimote_buffer[ingame_pad].Push(state);

    // add to packet
    AddWiimoteStateToPacket(ingame_pad, state, packet);
    data_added = true;
  }

  return data_added;
}

void NetPlayClient::SendPadHostPoll(const PadIndex pad_num)
{
  // Here we handle polling for the Host Input Authority and Golf modes. Pad data is "polled" from
  // the most recent data received for the given pad. Passing pad_num < 0 will poll all assigned
  // pads (used for batched polls), while 0..3 will poll the respective pad (used for MMIO polls).
  // See GetNetPads for more details.
  //
  // If the local buffer is non-empty, we skip actually buffering and sending new pad data, this way
  // don't end up with permanent local latency. It does create a period of time where no inputs are
  // accepted, but under typical circumstances this is not noticeable.
  //
  // Additionally, we wait until some actual pad data has been received before buffering and sending
  // it, otherwise controllers get calibrated wrongly with the default values of GCPadStatus.

  if (m_local_player->pid != m_current_golfer)
    return;

  sf::Packet packet;
  packet << MessageID::PadHostData;

  if (pad_num < 0)
  {
    for (size_t i = 0; i < m_pad_map.size(); i++)
    {
      if (m_pad_map[i] <= 0)
        continue;

      while (!m_first_pad_status_received[i])
      {
        if (!m_is_running.IsSet())
          return;

        m_first_pad_status_received_event.Wait();
      }
    }

    for (size_t i = 0; i < m_pad_map.size(); i++)
    {
      if (m_pad_map[i] == 0 || m_pad_buffer[i].Size() > 0)
        continue;

      const GCPadStatus& pad_status = m_last_pad_status[i];
      m_pad_buffer[i].Push(pad_status);
      AddPadStateToPacket(static_cast<int>(i), pad_status, packet);
    }
  }
  else if (m_pad_map[pad_num] != 0)
  {
    while (!m_first_pad_status_received[pad_num])
    {
      if (!m_is_running.IsSet())
        return;

      m_first_pad_status_received_event.Wait();
    }

    if (m_pad_buffer[pad_num].Size() == 0)
    {
      const GCPadStatus& pad_status = m_last_pad_status[pad_num];
      m_pad_buffer[pad_num].Push(pad_status);
      AddPadStateToPacket(pad_num, pad_status, packet);
    }
  }

  SendAsync(std::move(packet));
}

void NetPlayClient::InvokeStop()
{
  m_is_running.Clear();

  // stop waiting for input
  m_gc_pad_event.Set();
  m_wii_pad_event.Set();
  m_first_pad_status_received_event.Set();
  m_wait_on_input_event.Set();
}

// called from ---GUI--- thread and ---NETPLAY--- thread (client side)
bool NetPlayClient::StopGame()
{
  InvokeStop();

  NetPlay_Disable();

  // stop game
  m_dialog->StopGame();

  return true;
}

// called from ---GUI--- thread
void NetPlayClient::Stop()
{
  if (!m_is_running.IsSet())
    return;

  InvokeStop();

  // Tell the server to stop if we have a pad mapped in game.
  if (LocalPlayerHasControllerMapped())
    SendStopGamePacket();
  else
    StopGame();
}

void NetPlayClient::RequestStopGame()
{
  // Tell the server to stop if we have a pad mapped in game.
  if (LocalPlayerHasControllerMapped())
    SendStopGamePacket();
}

void NetPlayClient::SendPowerButtonEvent()
{
  sf::Packet packet;
  packet << MessageID::PowerButton;
  SendAsync(std::move(packet));
}

void NetPlayClient::RequestGolfControl(const PlayerId pid)
{
  if (!m_host_input_authority || !m_net_settings.golf_mode)
    return;

  sf::Packet packet;
  packet << MessageID::GolfRequest;
  packet << pid;
  SendAsync(std::move(packet));
}

void NetPlayClient::RequestGolfControl()
{
  RequestGolfControl(m_local_player->pid);
}

// called from ---GUI--- thread
std::string NetPlayClient::GetCurrentGolfer()
{
  std::lock_guard lkp(m_crit.players);
  if (m_players.count(m_current_golfer))
    return m_players[m_current_golfer].name;
  return "";
}

// called from ---GUI--- thread
bool NetPlayClient::LocalPlayerHasControllerMapped() const
{
  return PlayerHasControllerMapped(m_local_player->pid);
}

bool NetPlayClient::IsFirstInGamePad(int ingame_pad) const
{
  return std::none_of(m_pad_map.begin(), m_pad_map.begin() + ingame_pad,
                      [](auto mapping) { return mapping > 0; });
}

static int CountLocalPads(const PadMappingArray& pad_map, const PlayerId& local_player_pid)
{
  return static_cast<int>(
      std::count_if(pad_map.begin(), pad_map.end(), [&local_player_pid](const auto& mapping) {
        return mapping == local_player_pid;
      }));
}

int NetPlayClient::NumLocalPads() const
{
  return CountLocalPads(m_pad_map, m_local_player->pid);
}

int NetPlayClient::NumLocalWiimotes() const
{
  return CountLocalPads(m_wiimote_map, m_local_player->pid);
}

static int InGameToLocal(int ingame_pad, const PadMappingArray& pad_map, PlayerId local_player_pid)
{
  // not our pad
  if (pad_map[ingame_pad] != local_player_pid)
    return 4;

  int local_pad = 0;
  int pad = 0;

  for (; pad < ingame_pad; ++pad)
  {
    if (pad_map[pad] == local_player_pid)
      local_pad++;
  }

  return local_pad;
}

static int LocalToInGame(int local_pad, const PadMappingArray& pad_map, PlayerId local_player_pid)
{
  // Figure out which in-game pad maps to which local pad.
  // The logic we have here is that the local slots always
  // go in order.
  int local_pad_count = -1;
  int ingame_pad = 0;
  for (; ingame_pad < 4; ingame_pad++)
  {
    if (pad_map[ingame_pad] == local_player_pid)
      local_pad_count++;

    if (local_pad_count == local_pad)
      break;
  }

  return ingame_pad;
}

int NetPlayClient::InGamePadToLocalPad(int ingame_pad) const
{
  return InGameToLocal(ingame_pad, m_pad_map, m_local_player->pid);
}

int NetPlayClient::LocalPadToInGamePad(int local_pad) const
{
  return LocalToInGame(local_pad, m_pad_map, m_local_player->pid);
}

int NetPlayClient::InGameWiimoteToLocalWiimote(int ingame_wiimote) const
{
  return InGameToLocal(ingame_wiimote, m_wiimote_map, m_local_player->pid);
}

int NetPlayClient::LocalWiimoteToInGameWiimote(int local_wiimote) const
{
  return LocalToInGame(local_wiimote, m_wiimote_map, m_local_player->pid);
}

bool NetPlayClient::PlayerHasControllerMapped(const PlayerId pid) const
{
  const auto mapping_matches_player_id = [pid](const PlayerId& mapping) { return mapping == pid; };

  return std::any_of(m_pad_map.begin(), m_pad_map.end(), mapping_matches_player_id) ||
         std::any_of(m_wiimote_map.begin(), m_wiimote_map.end(), mapping_matches_player_id);
}

bool NetPlayClient::IsLocalPlayer(const PlayerId pid) const
{
  return pid == m_local_player->pid;
}

const PlayerId& NetPlayClient::GetLocalPlayerId() const
{
  return m_local_player->pid;
}

void NetPlayClient::SendGameStatus()
{
  sf::Packet packet;
  packet << MessageID::GameStatus;

  SyncIdentifierComparison result;
  m_dialog->FindGameFile(m_selected_game, &result);
  for (size_t i = 0; i < 4; ++i)
  {
    if (m_gba_config[i].enabled && m_gba_config[i].has_rom &&
        m_net_settings.gba_rom_paths[i].empty())
    {
      result = SyncIdentifierComparison::DifferentGame;
    }
  }

  packet << static_cast<u32>(result);
  Send(packet);
}

void NetPlayClient::SendTimeBase()
{
  std::lock_guard lk(crit_netplay_client);

  if (netplay_client->m_timebase_frame % 60 == 0)
  {
    const sf::Uint64 timebase = Core::System::GetInstance().GetSystemTimers().GetFakeTimeBase();

    sf::Packet packet;
    packet << MessageID::TimeBase;
    packet << timebase;
    packet << netplay_client->m_timebase_frame;

    netplay_client->SendAsync(std::move(packet));
  }

  netplay_client->m_timebase_frame++;
}

bool NetPlayClient::DoAllPlayersHaveGame()
{
  std::lock_guard lkp(m_crit.players);

  return std::all_of(std::begin(m_players), std::end(m_players), [](auto entry) {
    return entry.second.game_status == SyncIdentifierComparison::SameGame;
  });
}

static std::string SHA1Sum(const std::string& file_path, std::function<bool(int)> report_progress)
{
  std::vector<u8> data(8 * 1024 * 1024);
  u64 read_offset = 0;

  std::unique_ptr<DiscIO::BlobReader> file(DiscIO::CreateBlobReader(file_path));
  u64 game_size = file->GetDataSize();

  auto ctx = Common::SHA1::CreateContext();

  while (read_offset < game_size)
  {
    size_t read_size = std::min(static_cast<u64>(data.size()), game_size - read_offset);
    if (!file->Read(read_offset, read_size, data.data()))
      return "";

    ctx->Update(data.data(), read_size);
    read_offset += read_size;

    int progress =
        static_cast<int>(static_cast<float>(read_offset) / static_cast<float>(game_size) * 100);
    if (!report_progress(progress))
      return "";
  }

  // Convert to hex
  return fmt::format("{:02x}", fmt::join(ctx->Finish(), ""));
}

void NetPlayClient::ComputeGameDigest(const SyncIdentifier& sync_identifier)
{
  if (m_should_compute_game_digest)
    return;

  m_dialog->ShowGameDigestDialog(sync_identifier.game_id);
  m_should_compute_game_digest = true;

  std::string file;
  if (sync_identifier == GetSDCardIdentifier())
    file = File::GetUserPath(F_WIISDCARDIMAGE_IDX);
  else if (auto game = m_dialog->FindGameFile(sync_identifier))
    file = game->GetFilePath();

  if (file.empty() || !File::Exists(file))
  {
    sf::Packet packet;
    packet << MessageID::GameDigestError;
    packet << "file not found";
    Send(packet);
    return;
  }

  if (m_game_digest_thread.joinable())
    m_game_digest_thread.join();
  m_game_digest_thread = std::thread([this, file]() {
    std::string sum = SHA1Sum(file, [&](int progress) {
      sf::Packet packet;
      packet << MessageID::GameDigestProgress;
      packet << progress;
      SendAsync(std::move(packet));

      return m_should_compute_game_digest;
    });

    sf::Packet packet;
    packet << MessageID::GameDigestResult;
    packet << sum;
    SendAsync(std::move(packet));
  });
}

const PadMappingArray& NetPlayClient::GetPadMapping() const
{
  return m_pad_map;
}

const GBAConfigArray& NetPlayClient::GetGBAConfig() const
{
  return m_gba_config;
}

const PadMappingArray& NetPlayClient::GetWiimoteMapping() const
{
  return m_wiimote_map;
}

void NetPlayClient::AdjustPadBufferSize(const unsigned int size)
{
  m_target_buffer_size = size;
  m_dialog->OnPadBufferChanged(size);
}

void NetPlayClient::SetWiiSyncData(std::unique_ptr<IOS::HLE::FS::FileSystem> fs,
                                   std::vector<u64> titles, std::string redirect_folder)
{
  m_wii_sync_fs = std::move(fs);
  m_wii_sync_titles = std::move(titles);
  m_wii_sync_redirect_folder = std::move(redirect_folder);
}

SyncIdentifier NetPlayClient::GetSDCardIdentifier()
{
  return SyncIdentifier{{}, "sd", {}, {}, {}, {}};
}

std::string GetPlayerMappingString(PlayerId pid, const PadMappingArray& pad_map,
                                   const GBAConfigArray& gba_config,
                                   const PadMappingArray& wiimote_map)
{
  std::vector<size_t> gc_slots, gba_slots, wiimote_slots;
  for (size_t i = 0; i < pad_map.size(); ++i)
  {
    if (pad_map[i] == pid && !gba_config[i].enabled)
      gc_slots.push_back(i + 1);
    if (pad_map[i] == pid && gba_config[i].enabled)
      gba_slots.push_back(i + 1);
    if (wiimote_map[i] == pid)
      wiimote_slots.push_back(i + 1);
  }
  std::vector<std::string> groups;
  for (const auto& [group_name, slots] :
       {std::make_pair("GC", &gc_slots), std::make_pair("GBA", &gba_slots),
        std::make_pair("Wii", &wiimote_slots)})
  {
    if (!slots->empty())
      groups.emplace_back(fmt::format("{}{}", group_name, fmt::join(*slots, ",")));
  }
  std::string res = fmt::format("{}", fmt::join(groups, "|"));
  return res.empty() ? "None" : res;
}

bool IsNetPlayRunning()
{
  return netplay_client != nullptr;
}

void SetSIPollBatching(bool state)
{
  s_si_poll_batching = state;
}

void SendPowerButtonEvent()
{
  ASSERT(IsNetPlayRunning());
  netplay_client->SendPowerButtonEvent();
}

std::string GetGBASavePath(int pad_num)
{
  std::lock_guard lk(crit_netplay_client);

  if (!netplay_client || netplay_client->GetNetSettings().is_hosting)
  {
#ifdef HAS_LIBMGBA
    std::string rom_path = Config::Get(Config::MAIN_GBA_ROM_PATHS[pad_num]);
    return HW::GBA::Core::GetSavePath(rom_path, pad_num);
#else
    return {};
#endif
  }

  if (!netplay_client->GetNetSettings().savedata_load)
    return {};

  return fmt::format("{}{}{}.sav", File::GetUserPath(D_GBAUSER_IDX), GBA_SAVE_NETPLAY, pad_num + 1);
}

PadDetails GetPadDetails(int pad_num)
{
  std::lock_guard lk(crit_netplay_client);

  PadDetails res{};
  res.local_pad = 4;
  if (!netplay_client)
    return res;

  auto pad_map = netplay_client->GetPadMapping();
  if (pad_map[pad_num] <= 0)
    return res;

  for (auto player : netplay_client->GetPlayers())
  {
    if (player->pid == pad_map[pad_num])
      res.player_name = player->name;
  }

  int local_pad = 0;
  int non_local_pad = 0;
  for (int i = 0; i < pad_num; ++i)
  {
    if (netplay_client->IsLocalPlayer(pad_map[i]))
      ++local_pad;
    else
      ++non_local_pad;
  }
  res.is_local = netplay_client->IsLocalPlayer(pad_map[pad_num]);
  res.local_pad = res.is_local ? local_pad : netplay_client->NumLocalPads() + non_local_pad;
  res.hide_gba = !res.is_local && netplay_client->GetNetSettings().hide_remote_gbas &&
                 netplay_client->LocalPlayerHasControllerMapped();
  return res;
}

int NumLocalWiimotes()
{
  std::lock_guard lk(crit_netplay_client);
  if (netplay_client)
    return netplay_client->NumLocalWiimotes();
  return 0;
}

void NetPlay_Enable(NetPlayClient* const np)
{
  std::lock_guard lk(crit_netplay_client);
  netplay_client = np;
}

void NetPlay_Disable()
{
  std::lock_guard lk(crit_netplay_client);
  netplay_client = nullptr;
}
}  // namespace NetPlay

// stuff hacked into dolphin

// called from ---CPU--- thread
// Actual Core function which is called on every frame
bool SerialInterface::CSIDevice_GCController::NetPlay_GetInput(int pad_num, GCPadStatus* status)
{
  std::lock_guard lk(NetPlay::crit_netplay_client);

  if (NetPlay::netplay_client)
    return NetPlay::netplay_client->GetNetPads(pad_num, NetPlay::s_si_poll_batching, status);

  return false;
}

bool NetPlay::NetPlay_GetWiimoteData(const std::span<NetPlayClient::WiimoteDataBatchEntry>& entries)
{
  std::lock_guard lk(crit_netplay_client);

  if (netplay_client)
    return netplay_client->WiimoteUpdate(entries);

  return false;
}

unsigned int NetPlay::NetPlay_GetLocalWiimoteForSlot(unsigned int slot)
{
  if (slot >= std::tuple_size_v<PadMappingArray>)
    return slot;

  std::lock_guard lk(crit_netplay_client);

  if (!netplay_client)
    return slot;

  const auto& mapping = netplay_client->GetWiimoteMapping();
  const auto& local_player_id = netplay_client->GetLocalPlayerId();

  std::array<unsigned int, std::tuple_size_v<std::decay_t<decltype(mapping)>>> slot_map;
  size_t player_count = 0;
  for (size_t i = 0; i < mapping.size(); ++i)
  {
    if (mapping[i] == local_player_id)
    {
      slot_map[i] = static_cast<unsigned int>(player_count);
      ++player_count;
    }
  }
  for (size_t i = 0; i < mapping.size(); ++i)
  {
    if (mapping[i] != local_player_id)
    {
      slot_map[i] = static_cast<unsigned int>(player_count);
      ++player_count;
    }
  }

  INFO_LOG_FMT(NETPLAY, "Wiimote slot map: [{}]", fmt::join(slot_map, ", "));

  return slot_map[slot];
}

// called from ---CPU--- thread
// so all players' games get the same time
//
// also called from ---GUI--- thread when starting input recording
u64 ExpansionInterface::CEXIIPL::NetPlay_GetEmulatedTime()
{
  std::lock_guard lk(NetPlay::crit_netplay_client);

  if (NetPlay::netplay_client)
    return NetPlay::netplay_client->GetInitialRTCValue();

  return 0;
}

// called from ---CPU--- thread
// return the local pad num that should rumble given a ingame pad num
int SerialInterface::CSIDevice_GCController::NetPlay_InGamePadToLocalPad(int numPAD)
{
  std::lock_guard lk(NetPlay::crit_netplay_client);

  if (NetPlay::netplay_client)
    return NetPlay::netplay_client->InGamePadToLocalPad(numPAD);

  return numPAD;
}
