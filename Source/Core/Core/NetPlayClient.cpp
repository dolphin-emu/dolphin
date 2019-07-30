// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/NetPlayClient.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <type_traits>
#include <vector>

#include <lzo/lzo1x.h>
#include <mbedtls/md5.h>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/ENetUtil.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MD5.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/QoSSession.h"
#include "Common/SFMLHelper.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "Common/Version.h"
#include "Core/ActionReplay.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SI/SI_DeviceGCController.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiiSave.h"
#include "Core/HW/WiiSaveStructs.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/FS/HostBackend/FS.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/Uids.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PowerPC.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/GCAdapter.h"
#include "InputCommon/InputConfig.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

namespace NetPlay
{
using namespace WiimoteCommon;

static std::mutex crit_netplay_client;
static NetPlayClient* netplay_client = nullptr;
static std::unique_ptr<IOS::HLE::FS::FileSystem> s_wii_sync_fs;
static std::vector<u64> s_wii_sync_titles;
static bool s_si_poll_batching;

// called from ---GUI--- thread
NetPlayClient::~NetPlayClient()
{
  // not perfect
  if (m_is_running.IsSet())
    StopGame();

  if (m_is_connected)
  {
    m_should_compute_MD5 = false;
    m_dialog->AbortMD5();
    if (m_MD5_thread.joinable())
      m_MD5_thread.join();
    m_do_loop.Clear();
    m_thread.join();

    m_chunked_data_receive_queue.clear();
    m_dialog->HideChunkedProgressDialog();
  }

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

  if (m_traversal_client)
  {
    ReleaseTraversalClient();
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

    ENetAddress addr;
    enet_address_set_host(&addr, address.c_str());
    addr.port = port;

    m_server = enet_host_connect(m_client, &addr, CHANNEL_COUNT, 0);

    if (m_server == nullptr)
    {
      m_dialog->OnConnectionError(_trans("Could not create peer."));
      return;
    }

    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, 5000);
    if (net > 0 && netEvent.type == ENET_EVENT_TYPE_CONNECT)
    {
      if (Connect())
      {
        m_client->intercept = ENetUtil::InterceptCallback;
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
    if (address.size() > NETPLAY_CODE_SIZE)
    {
      m_dialog->OnConnectionError(
          _trans("The host code is too long.\nPlease recheck that you have the correct code."));
      return;
    }

    if (!EnsureTraversalClient(traversal_config.traversal_host, traversal_config.traversal_port))
      return;
    m_client = g_MainNetHost.get();

    m_traversal_client = g_TraversalClient.get();

    // If we were disconnected in the background, reconnect.
    if (m_traversal_client->GetState() == TraversalClient::Failure)
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
      if (connect_timer.GetTimeElapsed() > 5000)
        break;
    }
    m_dialog->OnConnectionError(_trans("Could not communicate with host."));
  }
}

bool NetPlayClient::Connect()
{
  // send connect message
  sf::Packet packet;
  packet << Common::scm_rev_git_str;
  packet << Common::netplay_dolphin_ver;
  packet << m_player_name;
  Send(packet);
  enet_host_flush(m_client);
  sf::Packet rpac;
  // TODO: make this not hang
  ENetEvent netEvent;
  if (enet_host_service(m_client, &netEvent, 5000) > 0 && netEvent.type == ENET_EVENT_TYPE_RECEIVE)
  {
    rpac.append(netEvent.packet->data, netEvent.packet->dataLength);
    enet_packet_destroy(netEvent.packet);
  }
  else
  {
    return false;
  }

  MessageId error;
  rpac >> error;

  // got error message
  if (error)
  {
    switch (error)
    {
    case CON_ERR_SERVER_FULL:
      m_dialog->OnConnectionError(_trans("The server is full."));
      break;
    case CON_ERR_VERSION_MISMATCH:
      m_dialog->OnConnectionError(
          _trans("The server and client's NetPlay versions are incompatible."));
      break;
    case CON_ERR_GAME_RUNNING:
      m_dialog->OnConnectionError(_trans("The game is currently running."));
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
    player.revision = Common::netplay_dolphin_ver;

    // add self to player list
    m_players[m_pid] = player;
    m_local_player = &m_players[m_pid];

    m_dialog->Update();

    m_is_connected = true;

    return true;
  }
}

// called from ---NETPLAY--- thread
unsigned int NetPlayClient::OnData(sf::Packet& packet)
{
  MessageId mid;
  packet >> mid;

  INFO_LOG(NETPLAY, "Got server message: %x", mid);

  switch (mid)
  {
  case NP_MSG_PLAYER_JOIN:
  {
    Player player;
    packet >> player.pid;
    packet >> player.name;
    packet >> player.revision;

    INFO_LOG(NETPLAY, "Player %s (%d) using %s joined", player.name.c_str(), player.pid,
             player.revision.c_str());

    {
      std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
      m_players[player.pid] = player;
    }

    m_dialog->Update();
  }
  break;

  case NP_MSG_PLAYER_LEAVE:
  {
    PlayerId pid;
    packet >> pid;

    INFO_LOG(NETPLAY, "Player %s (%d) left", m_players.find(pid)->second.name.c_str(), pid);

    {
      std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
      m_players.erase(m_players.find(pid));
    }

    m_dialog->Update();
  }
  break;

  case NP_MSG_CHAT_MESSAGE:
  {
    PlayerId pid;
    packet >> pid;
    std::string msg;
    packet >> msg;

    // don't need lock to read in this thread
    const Player& player = m_players[pid];

    INFO_LOG(NETPLAY, "Player %s (%d) wrote: %s", player.name.c_str(), player.pid, msg.c_str());

    // add to gui
    std::ostringstream ss;
    ss << player.name << '[' << (char)(pid + '0') << "]: " << msg;

    m_dialog->AppendChat(ss.str());
  }
  break;

  case NP_MSG_CHUNKED_DATA_START:
  {
    u32 cid;
    packet >> cid;
    std::string title;
    packet >> title;
    u64 data_size = Common::PacketReadU64(packet);

    m_chunked_data_receive_queue.emplace(cid, sf::Packet{});

    std::vector<int> players;
    players.push_back(m_local_player->pid);
    m_dialog->ShowChunkedProgressDialog(title, data_size, players);
  }
  break;

  case NP_MSG_CHUNKED_DATA_END:
  {
    u32 cid;
    packet >> cid;

    if (m_chunked_data_receive_queue.count(cid))
    {
      OnData(m_chunked_data_receive_queue[cid]);
      m_chunked_data_receive_queue.erase(cid);
      m_dialog->HideChunkedProgressDialog();

      sf::Packet complete_packet;
      complete_packet << static_cast<MessageId>(NP_MSG_CHUNKED_DATA_COMPLETE);
      complete_packet << cid;
      Send(complete_packet, CHUNKED_DATA_CHANNEL);
    }
  }
  break;

  case NP_MSG_CHUNKED_DATA_PAYLOAD:
  {
    u32 cid;
    packet >> cid;

    if (m_chunked_data_receive_queue.count(cid))
    {
      while (!packet.endOfPacket())
      {
        u8 byte;
        packet >> byte;
        m_chunked_data_receive_queue[cid] << byte;
      }

      m_dialog->SetChunkedProgress(m_local_player->pid,
                                   m_chunked_data_receive_queue[cid].getDataSize());

      sf::Packet progress_packet;
      progress_packet << static_cast<MessageId>(NP_MSG_CHUNKED_DATA_PROGRESS);
      progress_packet << cid;
      progress_packet << sf::Uint64{m_chunked_data_receive_queue[cid].getDataSize()};
      Send(progress_packet, CHUNKED_DATA_CHANNEL);
    }
  }
  break;

  case NP_MSG_CHUNKED_DATA_ABORT:
  {
    u32 cid;
    packet >> cid;

    if (m_chunked_data_receive_queue.count(cid))
    {
      m_chunked_data_receive_queue.erase(cid);
      m_dialog->HideChunkedProgressDialog();
    }
  }
  break;

  case NP_MSG_PAD_MAPPING:
  {
    for (PlayerId& mapping : m_pad_map)
    {
      packet >> mapping;
    }

    UpdateDevices();

    m_dialog->Update();
  }
  break;

  case NP_MSG_WIIMOTE_MAPPING:
  {
    for (PlayerId& mapping : m_wiimote_map)
    {
      packet >> mapping;
    }

    m_dialog->Update();
  }
  break;

  case NP_MSG_PAD_DATA:
  {
    while (!packet.endOfPacket())
    {
      PadIndex map;
      packet >> map;

      GCPadStatus pad;
      packet >> pad.button >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >>
          pad.substickX >> pad.substickY >> pad.triggerLeft >> pad.triggerRight >> pad.isConnected;

      // Trusting server for good map value (>=0 && <4)
      // add to pad buffer
      m_pad_buffer.at(map).Push(pad);
      m_gc_pad_event.Set();
    }
  }
  break;

  case NP_MSG_PAD_HOST_DATA:
  {
    while (!packet.endOfPacket())
    {
      PadIndex map;
      packet >> map;

      GCPadStatus pad;
      packet >> pad.button >> pad.analogA >> pad.analogB >> pad.stickX >> pad.stickY >>
          pad.substickX >> pad.substickY >> pad.triggerLeft >> pad.triggerRight >> pad.isConnected;

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
  break;

  case NP_MSG_WIIMOTE_DATA:
  {
    PadIndex map;
    NetWiimote nw;
    u8 size;
    packet >> map >> size;

    nw.resize(size);

    for (unsigned int i = 0; i < size; ++i)
      packet >> nw[i];

    // Trusting server for good map value (>=0 && <4)
    // add to Wiimote buffer
    m_wiimote_buffer.at(map).Push(nw);
    m_wii_pad_event.Set();
  }
  break;

  case NP_MSG_PAD_BUFFER:
  {
    u32 size = 0;
    packet >> size;

    m_target_buffer_size = size;
    m_dialog->OnPadBufferChanged(size);
  }
  break;

  case NP_MSG_HOST_INPUT_AUTHORITY:
  {
    packet >> m_host_input_authority;
    m_dialog->OnHostInputAuthorityChanged(m_host_input_authority);
  }
  break;

  case NP_MSG_GOLF_SWITCH:
  {
    PlayerId pid;
    packet >> pid;

    const PlayerId previous_golfer = m_current_golfer;
    m_current_golfer = pid;
    m_dialog->OnGolferChanged(m_local_player->pid == pid, pid != 0 ? m_players[pid].name : "");

    if (m_local_player->pid == previous_golfer)
    {
      sf::Packet spac;
      spac << static_cast<MessageId>(NP_MSG_GOLF_RELEASE);
      Send(spac);
    }
    else if (m_local_player->pid == pid)
    {
      sf::Packet spac;
      spac << static_cast<MessageId>(NP_MSG_GOLF_ACQUIRE);
      Send(spac);

      // Pads are already calibrated so we can just ignore this
      m_first_pad_status_received.fill(true);

      m_wait_on_input = false;
      m_wait_on_input_event.Set();
    }
  }
  break;

  case NP_MSG_GOLF_PREPARE:
  {
    m_wait_on_input_received = true;
    m_wait_on_input = true;
  }
  break;

  case NP_MSG_CHANGE_GAME:
  {
    {
      std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
      packet >> m_selected_game;
    }

    INFO_LOG(NETPLAY, "Game changed to %s", m_selected_game.c_str());

    // update gui
    m_dialog->OnMsgChangeGame(m_selected_game);

    sf::Packet game_status_packet;
    game_status_packet << static_cast<MessageId>(NP_MSG_GAME_STATUS);

    PlayerGameStatus status = m_dialog->FindGame(m_selected_game).empty() ?
                                  PlayerGameStatus::NotFound :
                                  PlayerGameStatus::Ok;

    game_status_packet << static_cast<u32>(status);
    Send(game_status_packet);

    sf::Packet ipl_status_packet;
    ipl_status_packet << static_cast<MessageId>(NP_MSG_IPL_STATUS);
    ipl_status_packet << ExpansionInterface::CEXIIPL::HasIPLDump();
    Send(ipl_status_packet);
  }
  break;

  case NP_MSG_GAME_STATUS:
  {
    PlayerId pid;
    packet >> pid;

    {
      std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
      Player& player = m_players[pid];
      u32 status;
      packet >> status;
      player.game_status = static_cast<PlayerGameStatus>(status);
    }

    m_dialog->Update();
  }
  break;

  case NP_MSG_START_GAME:
  {
    {
      std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
      packet >> m_current_game;
      packet >> m_net_settings.m_CPUthread;

      INFO_LOG(NETPLAY, "Start of game %s", m_selected_game.c_str());

      {
        std::underlying_type_t<PowerPC::CPUCore> core;
        if (packet >> core)
          m_net_settings.m_CPUcore = static_cast<PowerPC::CPUCore>(core);
        else
          m_net_settings.m_CPUcore = PowerPC::CPUCore::CachedInterpreter;
      }

      packet >> m_net_settings.m_EnableCheats;
      packet >> m_net_settings.m_SelectedLanguage;
      packet >> m_net_settings.m_OverrideGCLanguage;
      packet >> m_net_settings.m_ProgressiveScan;
      packet >> m_net_settings.m_PAL60;
      packet >> m_net_settings.m_DSPEnableJIT;
      packet >> m_net_settings.m_DSPHLE;
      packet >> m_net_settings.m_WriteToMemcard;
      packet >> m_net_settings.m_CopyWiiSave;
      packet >> m_net_settings.m_OCEnable;
      packet >> m_net_settings.m_OCFactor;
      packet >> m_net_settings.m_ReducePollingRate;

      for (auto& device : m_net_settings.m_EXIDevice)
      {
        int tmp;
        packet >> tmp;
        device = static_cast<ExpansionInterface::TEXIDevices>(tmp);
      }

      packet >> m_net_settings.m_EFBAccessEnable;
      packet >> m_net_settings.m_BBoxEnable;
      packet >> m_net_settings.m_ForceProgressive;
      packet >> m_net_settings.m_EFBToTextureEnable;
      packet >> m_net_settings.m_XFBToTextureEnable;
      packet >> m_net_settings.m_DisableCopyToVRAM;
      packet >> m_net_settings.m_ImmediateXFBEnable;
      packet >> m_net_settings.m_EFBEmulateFormatChanges;
      packet >> m_net_settings.m_SafeTextureCacheColorSamples;
      packet >> m_net_settings.m_PerfQueriesEnable;
      packet >> m_net_settings.m_FPRF;
      packet >> m_net_settings.m_AccurateNaNs;
      packet >> m_net_settings.m_SyncOnSkipIdle;
      packet >> m_net_settings.m_SyncGPU;
      packet >> m_net_settings.m_SyncGpuMaxDistance;
      packet >> m_net_settings.m_SyncGpuMinDistance;
      packet >> m_net_settings.m_SyncGpuOverclock;
      packet >> m_net_settings.m_JITFollowBranch;
      packet >> m_net_settings.m_FastDiscSpeed;
      packet >> m_net_settings.m_MMU;
      packet >> m_net_settings.m_Fastmem;
      packet >> m_net_settings.m_SkipIPL;
      packet >> m_net_settings.m_LoadIPLDump;
      packet >> m_net_settings.m_VertexRounding;
      packet >> m_net_settings.m_InternalResolution;
      packet >> m_net_settings.m_EFBScaledCopy;
      packet >> m_net_settings.m_FastDepthCalc;
      packet >> m_net_settings.m_EnablePixelLighting;
      packet >> m_net_settings.m_WidescreenHack;
      packet >> m_net_settings.m_ForceFiltering;
      packet >> m_net_settings.m_MaxAnisotropy;
      packet >> m_net_settings.m_ForceTrueColor;
      packet >> m_net_settings.m_DisableCopyFilter;
      packet >> m_net_settings.m_DisableFog;
      packet >> m_net_settings.m_ArbitraryMipmapDetection;
      packet >> m_net_settings.m_ArbitraryMipmapDetectionThreshold;
      packet >> m_net_settings.m_EnableGPUTextureDecoding;
      packet >> m_net_settings.m_DeferEFBCopies;
      packet >> m_net_settings.m_EFBAccessTileSize;
      packet >> m_net_settings.m_EFBAccessDeferInvalidation;
      packet >> m_net_settings.m_StrictSettingsSync;

      m_initial_rtc = Common::PacketReadU64(packet);

      packet >> m_net_settings.m_SyncSaveData;
      packet >> m_net_settings.m_SaveDataRegion;
      packet >> m_net_settings.m_SyncCodes;
      packet >> m_net_settings.m_SyncAllWiiSaves;

      for (int& extension : m_net_settings.m_WiimoteExtension)
        packet >> extension;

      packet >> m_net_settings.m_GolfMode;

      m_net_settings.m_IsHosting = m_local_player->IsHost();
      m_net_settings.m_HostInputAuthority = m_host_input_authority;
    }

    m_dialog->OnMsgStartGame();
  }
  break;

  case NP_MSG_STOP_GAME:
  case NP_MSG_DISABLE_GAME:
  {
    INFO_LOG(NETPLAY, "Game stopped");

    StopGame();
    m_dialog->OnMsgStopGame();
  }
  break;

  case NP_MSG_POWER_BUTTON:
  {
    m_dialog->OnMsgPowerButton();
  }
  break;

  case NP_MSG_PING:
  {
    u32 ping_key = 0;
    packet >> ping_key;

    sf::Packet response_packet;
    response_packet << static_cast<MessageId>(NP_MSG_PONG);
    response_packet << ping_key;

    Send(response_packet);
  }
  break;

  case NP_MSG_PLAYER_PING_DATA:
  {
    PlayerId pid;
    packet >> pid;

    {
      std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
      Player& player = m_players[pid];
      packet >> player.ping;
    }

    DisplayPlayersPing();
    m_dialog->Update();
  }
  break;

  case NP_MSG_DESYNC_DETECTED:
  {
    int pid_to_blame;
    u32 frame;
    packet >> pid_to_blame;
    packet >> frame;

    std::string player = "??";
    std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
    {
      auto it = m_players.find(pid_to_blame);
      if (it != m_players.end())
        player = it->second.name;
    }

    INFO_LOG(NETPLAY, "Player %s (%d) desynced!", player.c_str(), pid_to_blame);

    m_dialog->OnDesync(frame, player);
  }
  break;

  case NP_MSG_SYNC_GC_SRAM:
  {
    const size_t sram_settings_len = sizeof(g_SRAM) - offsetof(Sram, settings);
    u8 sram[sram_settings_len];
    for (size_t i = 0; i < sram_settings_len; ++i)
    {
      packet >> sram[i];
    }

    {
      std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
      memcpy(&g_SRAM.settings, sram, sram_settings_len);
      g_SRAM_netplay_initialized = true;
    }
  }
  break;

  case NP_MSG_SYNC_SAVE_DATA:
  {
    MessageId sub_id;
    packet >> sub_id;

    switch (sub_id)
    {
    case SYNC_SAVE_DATA_NOTIFY:
    {
      if (m_local_player->IsHost())
        return 0;

      packet >> m_sync_save_data_count;
      m_sync_save_data_success_count = 0;

      if (m_sync_save_data_count == 0)
        SyncSaveDataResponse(true);
      else
        m_dialog->AppendChat(Common::GetStringT("Synchronizing save data..."));
    }
    break;

    case SYNC_SAVE_DATA_RAW:
    {
      if (m_local_player->IsHost())
        return 0;

      bool is_slot_a;
      std::string region;
      bool mc251;
      packet >> is_slot_a >> region >> mc251;

      const std::string path = File::GetUserPath(D_GCUSER_IDX) + GC_MEMCARD_NETPLAY +
                               (is_slot_a ? "A." : "B.") + region + (mc251 ? ".251" : "") + ".raw";
      if (File::Exists(path) && !File::Delete(path))
      {
        PanicAlertT("Failed to delete NetPlay memory card. Verify your write permissions.");
        SyncSaveDataResponse(false);
        return 0;
      }

      const bool success = DecompressPacketIntoFile(packet, path);
      SyncSaveDataResponse(success);
    }
    break;

    case SYNC_SAVE_DATA_GCI:
    {
      if (m_local_player->IsHost())
        return 0;

      bool is_slot_a;
      u8 file_count;
      packet >> is_slot_a >> file_count;

      const std::string path = File::GetUserPath(D_GCUSER_IDX) + GC_MEMCARD_NETPLAY DIR_SEP +
                               StringFromFormat("Card %c", is_slot_a ? 'A' : 'B');

      if ((File::Exists(path) && !File::DeleteDirRecursively(path + DIR_SEP)) ||
          !File::CreateFullPath(path + DIR_SEP))
      {
        PanicAlertT("Failed to reset NetPlay GCI folder. Verify your write permissions.");
        SyncSaveDataResponse(false);
        return 0;
      }

      for (u8 i = 0; i < file_count; i++)
      {
        std::string file_name;
        packet >> file_name;

        if (!DecompressPacketIntoFile(packet, path + DIR_SEP + file_name))
        {
          SyncSaveDataResponse(false);
          return 0;
        }
      }

      SyncSaveDataResponse(true);
    }
    break;

    case SYNC_SAVE_DATA_WII:
    {
      if (m_local_player->IsHost())
        return 0;

      const std::string path = File::GetUserPath(D_USER_IDX) + "Wii" GC_MEMCARD_NETPLAY DIR_SEP;

      if (File::Exists(path) && !File::DeleteDirRecursively(path))
      {
        PanicAlertT("Failed to reset NetPlay NAND folder. Verify your write permissions.");
        SyncSaveDataResponse(false);
        return 0;
      }

      auto temp_fs = std::make_unique<IOS::HLE::FS::HostFileSystem>(path);
      std::vector<u64> titles;

      const IOS::HLE::FS::Modes fs_modes = {IOS::HLE::FS::Mode::ReadWrite,
                                            IOS::HLE::FS::Mode::ReadWrite,
                                            IOS::HLE::FS::Mode::ReadWrite};

      // Read the Mii data
      bool mii_data;
      packet >> mii_data;
      if (mii_data)
      {
        auto buffer = DecompressPacketIntoBuffer(packet);

        temp_fs->CreateDirectory(IOS::PID_KERNEL, IOS::PID_KERNEL, "/shared2/menu/FaceLib", 0,
                                 fs_modes);
        auto file = temp_fs->CreateAndOpenFile(IOS::PID_KERNEL, IOS::PID_KERNEL,
                                               Common::GetMiiDatabasePath(), fs_modes);

        if (!buffer || !file || !file->Write(buffer->data(), buffer->size()))
        {
          PanicAlertT("Failed to write Mii data.");
          SyncSaveDataResponse(false);
          return 0;
        }
      }

      // Read the saves
      u32 save_count;
      packet >> save_count;
      for (u32 n = 0; n < save_count; n++)
      {
        u64 title_id = Common::PacketReadU64(packet);
        titles.push_back(title_id);
        temp_fs->CreateDirectory(IOS::PID_KERNEL, IOS::PID_KERNEL,
                                 Common::GetTitleDataPath(title_id), 0, fs_modes);
        auto save = WiiSave::MakeNandStorage(temp_fs.get(), title_id);

        bool exists;
        packet >> exists;
        if (!exists)
          continue;

        // Header
        WiiSave::Header header;
        packet >> header.tid;
        packet >> header.banner_size;
        packet >> header.permissions;
        packet >> header.unk1;
        for (size_t i = 0; i < header.md5.size(); i++)
          packet >> header.md5[i];
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
        for (size_t i = 0; i < bk_header.unk3.size(); i++)
          packet >> bk_header.unk3[i];
        packet >> bk_header.tid;
        for (size_t i = 0; i < bk_header.mac_address.size(); i++)
          packet >> bk_header.mac_address[i];

        // Files
        std::vector<WiiSave::Storage::SaveFile> files;
        for (u32 i = 0; i < bk_header.number_of_files; i++)
        {
          WiiSave::Storage::SaveFile file;
          packet >> file.mode >> file.attributes;
          {
            u8 tmp;
            packet >> tmp;
            file.type = static_cast<WiiSave::Storage::SaveFile::Type>(tmp);
          }
          packet >> file.path;

          if (file.type == WiiSave::Storage::SaveFile::Type::File)
          {
            auto buffer = DecompressPacketIntoBuffer(packet);
            if (!buffer)
            {
              SyncSaveDataResponse(false);
              return 0;
            }

            file.data = std::move(*buffer);
          }

          files.push_back(std::move(file));
        }

        if (!save->WriteHeader(header) || !save->WriteBkHeader(bk_header) ||
            !save->WriteFiles(files))
        {
          PanicAlertT("Failed to write Wii save.");
          SyncSaveDataResponse(false);
          return 0;
        }
      }

      SetWiiSyncData(std::move(temp_fs), titles);
      SyncSaveDataResponse(true);
    }
    break;

    default:
      PanicAlertT("Unknown SYNC_SAVE_DATA message received with id: %d", sub_id);
      break;
    }
  }
  break;

  case NP_MSG_SYNC_CODES:
  {
    // Recieve Data Packet
    MessageId sub_id;
    packet >> sub_id;

    // Check Which Operation to Perform with This Packet
    switch (sub_id)
    {
    case SYNC_CODES_NOTIFY:
    {
      // Set both codes as unsynced
      m_sync_gecko_codes_complete = false;
      m_sync_ar_codes_complete = false;
    }
    break;

    case SYNC_CODES_NOTIFY_GECKO:
    {
      // Return if this is the host
      if (m_local_player->IsHost())
        return 0;

      // Receive Number of Codelines to Receive
      packet >> m_sync_gecko_codes_count;

      m_sync_gecko_codes_success_count = 0;

      NOTICE_LOG(ACTIONREPLAY, "Receiving %d Gecko codelines", m_sync_gecko_codes_count);

      // Check if no codes to sync, if so return as finished
      if (m_sync_gecko_codes_count == 0)
      {
        m_sync_gecko_codes_complete = true;
        SyncCodeResponse(true);
      }
      else
        m_dialog->AppendChat(Common::GetStringT("Synchronizing Gecko codes..."));
    }
    break;

    case SYNC_CODES_DATA_GECKO:
    {
      // Return if this is the host
      if (m_local_player->IsHost())
        return 0;

      // Create a synced code vector
      std::vector<Gecko::GeckoCode> synced_codes;
      // Create a GeckoCode
      Gecko::GeckoCode gcode;
      gcode = Gecko::GeckoCode();
      // Initialize gcode
      gcode.name = "Synced Codes";
      gcode.enabled = true;

      // Receive code contents from packet
      for (int i = 0; i < m_sync_gecko_codes_count; i++)
      {
        Gecko::GeckoCode::Code new_code;
        packet >> new_code.address;
        packet >> new_code.data;

        NOTICE_LOG(ACTIONREPLAY, "Received %08x %08x", new_code.address, new_code.data);

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
    break;

    case SYNC_CODES_NOTIFY_AR:
    {
      // Return if this is the host
      if (m_local_player->IsHost())
        return 0;

      // Receive Number of Codelines to Receive
      packet >> m_sync_ar_codes_count;

      m_sync_ar_codes_success_count = 0;

      NOTICE_LOG(ACTIONREPLAY, "Receiving %d AR codelines", m_sync_ar_codes_count);

      // Check if no codes to sync, if so return as finished
      if (m_sync_ar_codes_count == 0)
      {
        m_sync_ar_codes_complete = true;
        SyncCodeResponse(true);
      }
      else
        m_dialog->AppendChat(Common::GetStringT("Synchronizing AR codes..."));
    }
    break;

    case SYNC_CODES_DATA_AR:
    {
      // Return if this is the host
      if (m_local_player->IsHost())
        return 0;

      // Create a synced code vector
      std::vector<ActionReplay::ARCode> synced_codes;
      // Create an ARCode
      ActionReplay::ARCode arcode;
      arcode = ActionReplay::ARCode();
      // Initialize arcode
      arcode.name = "Synced Codes";
      arcode.active = true;

      // Receive code contents from packet
      for (int i = 0; i < m_sync_ar_codes_count; i++)
      {
        ActionReplay::AREntry new_code;
        packet >> new_code.cmd_addr;
        packet >> new_code.value;

        NOTICE_LOG(ACTIONREPLAY, "Received %08x %08x", new_code.cmd_addr, new_code.value);
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
    break;
    }
  }
  break;

  case NP_MSG_COMPUTE_MD5:
  {
    std::string file_identifier;
    packet >> file_identifier;

    ComputeMD5(file_identifier);
  }
  break;

  case NP_MSG_MD5_PROGRESS:
  {
    PlayerId pid;
    int progress;
    packet >> pid;
    packet >> progress;

    m_dialog->SetMD5Progress(pid, progress);
  }
  break;

  case NP_MSG_MD5_RESULT:
  {
    PlayerId pid;
    std::string result;
    packet >> pid;
    packet >> result;

    m_dialog->SetMD5Result(pid, result);
  }
  break;

  case NP_MSG_MD5_ERROR:
  {
    PlayerId pid;
    std::string error;
    packet >> pid;
    packet >> error;

    m_dialog->SetMD5Result(pid, error);
  }
  break;

  case NP_MSG_MD5_ABORT:
  {
    m_should_compute_MD5 = false;
    m_dialog->AbortMD5();
  }
  break;

  default:
    PanicAlertT("Unknown message received with id : %d", mid);
    break;
  }

  return 0;
}

void NetPlayClient::Send(const sf::Packet& packet, const u8 channel_id)
{
  ENetPacket* epac =
      enet_packet_create(packet.getData(), packet.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(m_server, channel_id, epac);
}

void NetPlayClient::DisplayPlayersPing()
{
  if (!g_ActiveConfig.bShowNetPlayPing)
    return;

  OSD::AddTypedMessage(OSD::MessageType::NetPlayPing,
                       StringFromFormat("Ping: %u", GetPlayersMaxPing()), OSD::Duration::SHORT,
                       OSD::Color::CYAN);
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
    std::lock_guard<std::recursive_mutex> lkq(m_crit.async_queue_write);
    m_async_queue.Push(AsyncQueueEntry{std::move(packet), channel_id});
  }
  ENetUtil::WakeupThread(m_client);
}

// called from ---NETPLAY--- thread
void NetPlayClient::ThreadFunc()
{
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
      {
        auto& e = m_async_queue.Front();
        Send(e.packet, e.channel_id);
      }
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
        m_dialog->OnConnectionLost();

        if (m_is_running.IsSet())
          StopGame();

        break;
      default:
        break;
      }
    }
  }

  Disconnect();
  return;
}

// called from ---GUI--- thread
void NetPlayClient::GetPlayerList(std::string& list, std::vector<int>& pid_list)
{
  std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

  std::ostringstream ss;

  const auto enumerate_player_controller_mappings = [&ss](const PadMappingArray& mappings,
                                                          const Player& player) {
    for (size_t i = 0; i < mappings.size(); i++)
    {
      if (mappings[i] == player.pid)
        ss << i + 1;
      else
        ss << '-';
    }
  };

  for (const auto& entry : m_players)
  {
    const Player& player = entry.second;
    ss << player.name << "[" << static_cast<int>(player.pid) << "] : " << player.revision << " | ";

    enumerate_player_controller_mappings(m_pad_map, player);
    enumerate_player_controller_mappings(m_wiimote_map, player);

    ss << " |\nPing: " << player.ping << "ms\n";
    ss << "Status: ";

    switch (player.game_status)
    {
    case PlayerGameStatus::Ok:
      ss << "ready";
      break;

    case PlayerGameStatus::NotFound:
      ss << "game missing";
      break;

    default:
      ss << "unknown";
      break;
    }

    ss << "\n\n";

    pid_list.push_back(player.pid);
  }

  list = ss.str();
}

// called from ---GUI--- thread
std::vector<const Player*> NetPlayClient::GetPlayers()
{
  std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
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
  packet << static_cast<MessageId>(NP_MSG_CHAT_MESSAGE);
  packet << msg;

  SendAsync(std::move(packet));
}

// called from ---CPU--- thread
void NetPlayClient::AddPadStateToPacket(const int in_game_pad, const GCPadStatus& pad,
                                        sf::Packet& packet)
{
  packet << static_cast<PadIndex>(in_game_pad);
  packet << pad.button << pad.analogA << pad.analogB << pad.stickX << pad.stickY << pad.substickX
         << pad.substickY << pad.triggerLeft << pad.triggerRight << pad.isConnected;
}

// called from ---CPU--- thread
void NetPlayClient::SendWiimoteState(const int in_game_pad, const NetWiimote& nw)
{
  sf::Packet packet;
  packet << static_cast<MessageId>(NP_MSG_WIIMOTE_DATA);
  packet << static_cast<PadIndex>(in_game_pad);
  packet << static_cast<u8>(nw.size());
  for (auto it : nw)
  {
    packet << it;
  }

  SendAsync(std::move(packet));
}

// called from ---GUI--- thread
void NetPlayClient::SendStartGamePacket()
{
  sf::Packet packet;
  packet << static_cast<MessageId>(NP_MSG_START_GAME);
  packet << m_current_game;

  SendAsync(std::move(packet));
}

// called from ---GUI--- thread
void NetPlayClient::SendStopGamePacket()
{
  sf::Packet packet;
  packet << static_cast<MessageId>(NP_MSG_STOP_GAME);

  SendAsync(std::move(packet));
}

// called from ---GUI--- thread
bool NetPlayClient::StartGame(const std::string& path)
{
  std::lock_guard<std::recursive_mutex> lkg(m_crit.game);
  SendStartGamePacket();

  if (m_is_running.IsSet())
  {
    PanicAlertT("Game is already running!");
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
    if (Movie::IsReadOnly())
      Movie::SetReadOnly(false);

    u8 controllers_mask = 0;
    for (unsigned int i = 0; i < 4; ++i)
    {
      if (m_pad_map[i] > 0)
        controllers_mask |= (1 << i);
      if (m_wiimote_map[i] > 0)
        controllers_mask |= (1 << (i + 4));
    }
    Movie::BeginRecordingInput(controllers_mask);
  }

  for (unsigned int i = 0; i < 4; ++i)
    WiimoteReal::ChangeWiimoteSource(i, m_wiimote_map[i] > 0 ? WIIMOTE_SRC_EMU : WIIMOTE_SRC_NONE);

  // boot game
  m_dialog->BootGame(path);

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
      response_packet << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
      response_packet << static_cast<MessageId>(SYNC_SAVE_DATA_SUCCESS);

      Send(response_packet);
    }
  }
  else
  {
    sf::Packet response_packet;
    response_packet << static_cast<MessageId>(NP_MSG_SYNC_SAVE_DATA);
    response_packet << static_cast<MessageId>(SYNC_SAVE_DATA_FAILURE);

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
    response_packet << static_cast<MessageId>(NP_MSG_SYNC_CODES);
    response_packet << static_cast<MessageId>(SYNC_CODES_FAILURE);

    Send(response_packet);
    return;
  }

  // If both gecko and AR codes have completely finished transferring, report back as successful
  if (m_sync_gecko_codes_complete && m_sync_ar_codes_complete)
  {
    m_dialog->AppendChat(Common::GetStringT("Codes received!"));

    sf::Packet response_packet;
    response_packet << static_cast<MessageId>(NP_MSG_SYNC_CODES);
    response_packet << static_cast<MessageId>(SYNC_CODES_SUCCESS);

    Send(response_packet);
  }
}

bool NetPlayClient::DecompressPacketIntoFile(sf::Packet& packet, const std::string& file_path)
{
  u64 file_size = Common::PacketReadU64(packet);

  if (file_size == 0)
    return true;

  File::IOFile file(file_path, "wb");
  if (!file)
  {
    PanicAlertT("Failed to open file \"%s\". Verify your write permissions.", file_path.c_str());
    return false;
  }

  std::vector<u8> in_buffer(NETPLAY_LZO_OUT_LEN);
  std::vector<u8> out_buffer(NETPLAY_LZO_IN_LEN);

  while (true)
  {
    lzo_uint32 cur_len = 0;  // number of bytes to read
    lzo_uint new_len = 0;    // number of bytes to write

    packet >> cur_len;
    if (!cur_len)
      break;  // We reached the end of the data stream

    for (size_t j = 0; j < cur_len; j++)
    {
      packet >> in_buffer[j];
    }

    if (lzo1x_decompress(in_buffer.data(), cur_len, out_buffer.data(), &new_len, nullptr) !=
        LZO_E_OK)
    {
      PanicAlertT("Internal LZO Error - decompression failed");
      return false;
    }

    if (!file.WriteBytes(out_buffer.data(), new_len))
    {
      PanicAlertT("Error writing file: %s", file_path.c_str());
      return false;
    }
  }

  return true;
}

std::optional<std::vector<u8>> NetPlayClient::DecompressPacketIntoBuffer(sf::Packet& packet)
{
  u64 size = Common::PacketReadU64(packet);

  std::vector<u8> out_buffer(size);

  if (size == 0)
    return out_buffer;

  std::vector<u8> in_buffer(NETPLAY_LZO_OUT_LEN);

  lzo_uint i = 0;
  while (true)
  {
    lzo_uint32 cur_len = 0;  // number of bytes to read
    lzo_uint new_len = 0;    // number of bytes to write

    packet >> cur_len;
    if (!cur_len)
      break;  // We reached the end of the data stream

    for (size_t j = 0; j < cur_len; j++)
    {
      packet >> in_buffer[j];
    }

    if (lzo1x_decompress(in_buffer.data(), cur_len, &out_buffer[i], &new_len, nullptr) != LZO_E_OK)
    {
      PanicAlertT("Internal LZO Error - decompression failed");
      return {};
    }

    i += new_len;
  }

  return out_buffer;
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

  for (auto player_id : m_pad_map)
  {
    // Use local controller types for local controllers if they are compatible
    // Only GCController-like controllers are supported, GBA and similar
    // exotic devices are not supported on netplay.
    if (player_id == m_local_player->pid)
    {
      if (SerialInterface::SIDevice_IsGCController(SConfig::GetInstance().m_SIDevice[local_pad]))
      {
        SerialInterface::ChangeDevice(SConfig::GetInstance().m_SIDevice[local_pad], pad);

        if (SConfig::GetInstance().m_SIDevice[local_pad] == SerialInterface::SIDEVICE_WIIU_ADAPTER)
        {
          GCAdapter::ResetDeviceType(local_pad);
        }
      }
      else
      {
        SerialInterface::ChangeDevice(SerialInterface::SIDEVICE_GC_CONTROLLER, pad);
      }
      local_pad++;
    }
    else if (player_id > 0)
    {
      SerialInterface::ChangeDevice(SerialInterface::SIDEVICE_GC_CONTROLLER, pad);
    }
    else
    {
      SerialInterface::ChangeDevice(SerialInterface::SIDEVICE_NONE, pad);
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
  const TraversalClient::State state = m_traversal_client->GetState();

  if (m_connection_state == ConnectionState::WaitingForTraversalClientConnection &&
      state == TraversalClient::Connected)
  {
    m_connection_state = ConnectionState::WaitingForTraversalClientConnectReady;
    m_traversal_client->ConnectToClient(m_host_spec);
  }
  else if (m_connection_state != ConnectionState::Failure && state == TraversalClient::Failure)
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
void NetPlayClient::OnConnectFailed(u8 reason)
{
  m_connecting = false;
  m_connection_state = ConnectionState::Failure;
  switch (reason)
  {
  case TraversalConnectFailedClientDidntRespond:
    PanicAlertT("Traversal server timed out connecting to the host");
    break;
  case TraversalConnectFailedClientFailure:
    PanicAlertT("Server rejected traversal attempt");
    break;
  case TraversalConnectFailedNoSuchClient:
    PanicAlertT("Invalid host");
    break;
  default:
    PanicAlertT("Unknown error %x", reason);
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
      spac << static_cast<MessageId>(NP_MSG_GOLF_PREPARE);
      Send(spac);

      m_wait_on_input_received = false;
    }

    m_wait_on_input_event.Wait();
  }

  if (IsFirstInGamePad(pad_nb) && batching)
  {
    sf::Packet packet;
    packet << static_cast<MessageId>(NP_MSG_PAD_DATA);

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
    int local_pad = InGamePadToLocalPad(pad_nb);
    if (local_pad < 4)
    {
      sf::Packet packet;
      packet << static_cast<MessageId>(NP_MSG_PAD_DATA);
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
        SConfig::GetInstance().m_EmulationSpeed = buffer_over_target ? 0.0f : 1.0f;
      }
    }
    else
    {
      // Set normal speed when we're the host, otherwise it can get stuck at unlimited
      SConfig::GetInstance().m_EmulationSpeed = 1.0f;
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

  if (Movie::IsRecordingInput())
  {
    Movie::RecordInput(pad_status, pad_nb);
    Movie::InputUpdate();
  }
  else
  {
    Movie::CheckPadStatus(pad_status, pad_nb);
  }

  return true;
}

u64 NetPlayClient::GetInitialRTCValue() const
{
  return m_initial_rtc;
}

// called from ---CPU--- thread
bool NetPlayClient::WiimoteUpdate(int _number, u8* data, const u8 size, u8 reporting_mode)
{
  NetWiimote nw;
  {
    std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

    // Only send data, if this Wiimote is mapped to this player
    if (m_wiimote_map[_number] == m_local_player->pid)
    {
      nw.assign(data, data + size);
      do
      {
        // add to buffer
        m_wiimote_buffer[_number].Push(nw);

        SendWiimoteState(_number, nw);
      } while (m_wiimote_buffer[_number].Size() <=
               m_target_buffer_size * 200 /
                   120);  // TODO: add a seperate setting for wiimote buffer?
    }

  }  // unlock players

  while (m_wiimote_buffer[_number].Size() == 0)
  {
    if (!m_is_running.IsSet())
    {
      return false;
    }

    // wait for receiving thread to push some data
    m_wii_pad_event.Wait();
  }

  m_wiimote_buffer[_number].Pop(nw);

  // If the reporting mode has changed, we just need to pop through the buffer,
  // until we reach a good input
  if (nw[1] != reporting_mode)
  {
    u32 tries = 0;
    while (nw[1] != reporting_mode)
    {
      while (m_wiimote_buffer[_number].Size() == 0)
      {
        if (!m_is_running.IsSet())
        {
          return false;
        }

        // wait for receiving thread to push some data
        m_wii_pad_event.Wait();
      }

      m_wiimote_buffer[_number].Pop(nw);

      ++tries;
      if (tries > m_target_buffer_size * 200 / 120)
        break;
    }

    // If it still mismatches, it surely desynced
    if (nw[1] != reporting_mode)
    {
      PanicAlertT("Netplay has desynced. There is no way to recover from this.");
      return false;
    }
  }

  memcpy(data, nw.data(), size);
  return true;
}

bool NetPlayClient::PollLocalPad(const int local_pad, sf::Packet& packet)
{
  GCPadStatus pad_status;

  switch (SConfig::GetInstance().m_SIDevice[local_pad])
  {
  case SerialInterface::SIDEVICE_WIIU_ADAPTER:
    pad_status = GCAdapter::Input(local_pad);
    break;
  case SerialInterface::SIDEVICE_GC_CONTROLLER:
  default:
    pad_status = Pad::GetStatus(local_pad);
    break;
  }

  const int ingame_pad = LocalPadToInGamePad(local_pad);
  bool data_added = false;

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

void NetPlayClient::SendPadHostPoll(const PadIndex pad_num)
{
  if (m_local_player->pid != m_current_golfer)
    return;

  sf::Packet packet;
  packet << static_cast<MessageId>(NP_MSG_PAD_HOST_DATA);

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

// called from ---GUI--- thread and ---NETPLAY--- thread (client side)
bool NetPlayClient::StopGame()
{
  m_is_running.Clear();

  // stop waiting for input
  m_gc_pad_event.Set();
  m_wii_pad_event.Set();
  m_first_pad_status_received_event.Set();
  m_wait_on_input_event.Set();

  NetPlay_Disable();

  // stop game
  m_dialog->StopGame();

  ClearWiiSyncData();

  return true;
}

// called from ---GUI--- thread
void NetPlayClient::Stop()
{
  if (!m_is_running.IsSet())
    return;

  m_is_running.Clear();

  // stop waiting for input
  m_gc_pad_event.Set();
  m_wii_pad_event.Set();
  m_first_pad_status_received_event.Set();
  m_wait_on_input_event.Set();

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
  packet << static_cast<MessageId>(NP_MSG_POWER_BUTTON);
  SendAsync(std::move(packet));
}

void NetPlayClient::RequestGolfControl(const PlayerId pid)
{
  if (!m_host_input_authority || !m_net_settings.m_GolfMode)
    return;

  sf::Packet packet;
  packet << static_cast<MessageId>(NP_MSG_GOLF_REQUEST);
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
  std::lock_guard<std::recursive_mutex> lkp(m_crit.players);
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

int NetPlayClient::NumLocalPads() const
{
  return static_cast<int>(std::count_if(m_pad_map.begin(), m_pad_map.end(), [this](auto mapping) {
    return mapping == m_local_player->pid;
  }));
}

int NetPlayClient::InGamePadToLocalPad(int ingame_pad) const
{
  // not our pad
  if (m_pad_map[ingame_pad] != m_local_player->pid)
    return 4;

  int local_pad = 0;
  int pad = 0;

  for (; pad < ingame_pad; pad++)
  {
    if (m_pad_map[pad] == m_local_player->pid)
      local_pad++;
  }

  return local_pad;
}

int NetPlayClient::LocalPadToInGamePad(int local_pad) const
{
  // Figure out which in-game pad maps to which local pad.
  // The logic we have here is that the local slots always
  // go in order.
  int local_pad_count = -1;
  int ingame_pad = 0;
  for (; ingame_pad < 4; ingame_pad++)
  {
    if (m_pad_map[ingame_pad] == m_local_player->pid)
      local_pad_count++;

    if (local_pad_count == local_pad)
      break;
  }

  return ingame_pad;
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

void NetPlayClient::SendTimeBase()
{
  std::lock_guard<std::mutex> lk(crit_netplay_client);

  if (netplay_client->m_timebase_frame % 60 == 0)
  {
    const sf::Uint64 timebase = SystemTimers::GetFakeTimeBase();

    sf::Packet packet;
    packet << static_cast<MessageId>(NP_MSG_TIMEBASE);
    packet << timebase;
    packet << netplay_client->m_timebase_frame;

    netplay_client->SendAsync(std::move(packet));
  }

  netplay_client->m_timebase_frame++;
}

bool NetPlayClient::DoAllPlayersHaveGame()
{
  std::lock_guard<std::recursive_mutex> lkp(m_crit.players);

  return std::all_of(std::begin(m_players), std::end(m_players),
                     [](auto entry) { return entry.second.game_status == PlayerGameStatus::Ok; });
}

void NetPlayClient::ComputeMD5(const std::string& file_identifier)
{
  if (m_should_compute_MD5)
    return;

  m_dialog->ShowMD5Dialog(file_identifier);
  m_should_compute_MD5 = true;

  std::string file;
  if (file_identifier == WII_SDCARD)
    file = File::GetUserPath(F_WIISDCARD_IDX);
  else
    file = m_dialog->FindGame(file_identifier);

  if (file.empty() || !File::Exists(file))
  {
    sf::Packet packet;
    packet << static_cast<MessageId>(NP_MSG_MD5_ERROR);
    packet << "file not found";
    Send(packet);
    return;
  }

  if (m_MD5_thread.joinable())
    m_MD5_thread.join();
  m_MD5_thread = std::thread([this, file]() {
    std::string sum = MD5::MD5Sum(file, [&](int progress) {
      sf::Packet packet;
      packet << static_cast<MessageId>(NP_MSG_MD5_PROGRESS);
      packet << progress;
      SendAsync(std::move(packet));

      return m_should_compute_MD5;
    });

    sf::Packet packet;
    packet << static_cast<MessageId>(NP_MSG_MD5_RESULT);
    packet << sum;
    SendAsync(std::move(packet));
  });
}

const PadMappingArray& NetPlayClient::GetPadMapping() const
{
  return m_pad_map;
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

bool IsNetPlayRunning()
{
  return netplay_client != nullptr;
}

const NetSettings& GetNetSettings()
{
  ASSERT(IsNetPlayRunning());
  return netplay_client->GetNetSettings();
}

IOS::HLE::FS::FileSystem* GetWiiSyncFS()
{
  return s_wii_sync_fs.get();
}

const std::vector<u64>& GetWiiSyncTitles()
{
  return s_wii_sync_titles;
}

void SetWiiSyncData(std::unique_ptr<IOS::HLE::FS::FileSystem> fs, const std::vector<u64>& titles)
{
  s_wii_sync_fs = std::move(fs);
  s_wii_sync_titles.insert(s_wii_sync_titles.end(), titles.begin(), titles.end());
}

void ClearWiiSyncData()
{
  // We're just assuming it will always be here because it is
  const std::string path = File::GetUserPath(D_USER_IDX) + "Wii" GC_MEMCARD_NETPLAY DIR_SEP;
  if (File::Exists(path))
    File::DeleteDirRecursively(path);

  s_wii_sync_fs.reset();
  s_wii_sync_titles.clear();
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

bool IsSyncingAllWiiSaves()
{
  std::lock_guard<std::mutex> lk(crit_netplay_client);

  if (netplay_client)
    return netplay_client->GetNetSettings().m_SyncAllWiiSaves;

  return false;
}

void SetupWiimotes()
{
  ASSERT(IsNetPlayRunning());
  const NetSettings& netplay_settings = netplay_client->GetNetSettings();
  const PadMappingArray& wiimote_map = netplay_client->GetWiimoteMapping();
  for (size_t i = 0; i < netplay_settings.m_WiimoteExtension.size(); i++)
  {
    if (wiimote_map[i] > 0)
    {
      static_cast<ControllerEmu::Attachments*>(
          static_cast<WiimoteEmu::Wiimote*>(Wiimote::GetConfig()->GetController(int(i)))
              ->GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Attachments))
          ->SetSelectedAttachment(netplay_settings.m_WiimoteExtension[i]);
    }
  }
}

void NetPlay_Enable(NetPlayClient* const np)
{
  std::lock_guard<std::mutex> lk(crit_netplay_client);
  netplay_client = np;
}

void NetPlay_Disable()
{
  std::lock_guard<std::mutex> lk(crit_netplay_client);
  netplay_client = nullptr;
}
}  // namespace NetPlay

// stuff hacked into dolphin

// called from ---CPU--- thread
// Actual Core function which is called on every frame
bool SerialInterface::CSIDevice_GCController::NetPlay_GetInput(int pad_num, GCPadStatus* status)
{
  std::lock_guard<std::mutex> lk(NetPlay::crit_netplay_client);

  if (NetPlay::netplay_client)
    return NetPlay::netplay_client->GetNetPads(pad_num, NetPlay::s_si_poll_batching, status);

  return false;
}

bool WiimoteEmu::Wiimote::NetPlay_GetWiimoteData(int wiimote, u8* data, u8 size, u8 reporting_mode)
{
  std::lock_guard<std::mutex> lk(NetPlay::crit_netplay_client);

  if (NetPlay::netplay_client)
    return NetPlay::netplay_client->WiimoteUpdate(wiimote, data, size, reporting_mode);

  return false;
}

// Sync the info whether a button was pressed or not. Used for the reconnect on button press feature
bool Wiimote::NetPlay_GetButtonPress(int wiimote, bool pressed)
{
  std::lock_guard<std::mutex> lk(NetPlay::crit_netplay_client);

  // Use the reporting mode 0 for the button pressed event, the real ones start at RT_REPORT_CORE
  u8 data[2] = {static_cast<u8>(pressed), 0};

  if (NetPlay::netplay_client)
  {
    if (NetPlay::netplay_client->WiimoteUpdate(wiimote, data, 2, 0))
    {
      return data[0];
    }
    PanicAlertT("Netplay has desynced in NetPlay_GetButtonPress()");
    return false;
  }

  return pressed;
}

// called from ---CPU--- thread
// so all players' games get the same time
//
// also called from ---GUI--- thread when starting input recording
u64 ExpansionInterface::CEXIIPL::NetPlay_GetEmulatedTime()
{
  std::lock_guard<std::mutex> lk(NetPlay::crit_netplay_client);

  if (NetPlay::netplay_client)
    return NetPlay::netplay_client->GetInitialRTCValue();

  return 0;
}

// called from ---CPU--- thread
// return the local pad num that should rumble given a ingame pad num
int SerialInterface::CSIDevice_GCController::NetPlay_InGamePadToLocalPad(int numPAD)
{
  std::lock_guard<std::mutex> lk(NetPlay::crit_netplay_client);

  if (NetPlay::netplay_client)
    return NetPlay::netplay_client->InGamePadToLocalPad(numPAD);

  return numPAD;
}
