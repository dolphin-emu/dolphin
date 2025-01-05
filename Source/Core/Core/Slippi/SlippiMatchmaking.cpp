#include "SlippiMatchmaking.h"
#include <string>
#include <vector>
#include "Common/Common.h"
#include "Common/ENet.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

#if defined __linux__ && HAVE_ALSA
#elif defined __APPLE__
#include <arpa/inet.h>
#include <netdb.h>
#elif defined _WIN32
#endif

class MmMessageType
{
public:
  static std::string CREATE_TICKET;
  static std::string CREATE_TICKET_RESP;
  static std::string GET_TICKET_RESP;
};

std::string MmMessageType::CREATE_TICKET = "create-ticket";
std::string MmMessageType::CREATE_TICKET_RESP = "create-ticket-resp";
std::string MmMessageType::GET_TICKET_RESP = "get-ticket-resp";

SlippiMatchmaking::SlippiMatchmaking(SlippiUser* user)
{
  m_user = user;
  m_state = ProcessState::IDLE;
  m_error_msg = "";

  m_client = nullptr;
  m_server = nullptr;

  MM_HOST = Common::GetSemVerStr().find("dev") == std::string::npos ? MM_HOST_PROD : MM_HOST_DEV;

  generator = std::default_random_engine(Common::Timer::NowMs());
}

SlippiMatchmaking::~SlippiMatchmaking()
{
  is_mm_terminated = true;
  m_state = ProcessState::ERROR_ENCOUNTERED;
  m_error_msg = "Matchmaking shut down";

  if (m_matchmake_thread.joinable())
    m_matchmake_thread.join();

  terminateMmConnection();
}

void SlippiMatchmaking::FindMatch(MatchSearchSettings settings)
{
  is_mm_connected = false;

  ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Starting matchmaking...");

  m_search_settings = settings;

  m_error_msg = "";
  m_state = ProcessState::INITIALIZING;
  m_matchmake_thread = std::thread(&SlippiMatchmaking::MatchmakeThread, this);
}

SlippiMatchmaking::ProcessState SlippiMatchmaking::GetMatchmakeState()
{
  return m_state;
}

std::string SlippiMatchmaking::GetErrorMessage()
{
  return m_error_msg;
}

bool SlippiMatchmaking::IsSearching()
{
  return searching_states.count(m_state) != 0;
}

std::unique_ptr<SlippiNetplayClient> SlippiMatchmaking::GetNetplayClient()
{
  return std::move(m_netplay_client);
}

bool SlippiMatchmaking::IsFixedRulesMode(SlippiMatchmaking::OnlinePlayMode mode)
{
  return mode == SlippiMatchmaking::OnlinePlayMode::UNRANKED ||
         mode == SlippiMatchmaking::OnlinePlayMode::RANKED;
}

void SlippiMatchmaking::sendMessage(json msg)
{
  enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
  u8 channel_id = 0;

  std::string msgContents = msg.dump();

  ENetPacket* epac = enet_packet_create(msgContents.c_str(), msgContents.length(), flags);
  enet_peer_send(m_server, channel_id, epac);
}

int SlippiMatchmaking::receiveMessage(json& msg, int timeout_ms)
{
  int host_service_timeout_ms = 250;

  // Make sure loop runs at least once
  if (timeout_ms < host_service_timeout_ms)
    timeout_ms = host_service_timeout_ms;

  // This is not a perfect way to timeout but hopefully it's close enough?
  int max_attempts = timeout_ms / host_service_timeout_ms;

  for (int i = 0; i < max_attempts; i++)
  {
    ENetEvent net_event;
    int net = enet_host_service(m_client, &net_event, host_service_timeout_ms);
    if (net <= 0)
      continue;

    switch (net_event.type)
    {
    case ENET_EVENT_TYPE_RECEIVE:
    {
      std::vector<u8> buf;
      buf.insert(buf.end(), net_event.packet->data,
                 net_event.packet->data + net_event.packet->dataLength);

      std::string str(buf.begin(), buf.end());
      msg = json::parse(str, nullptr, false);

      enet_packet_destroy(net_event.packet);
      return 0;
    }
    case ENET_EVENT_TYPE_DISCONNECT:
      // Return -2 code to indicate we have lost connection to the server
      return -2;
    }
  }

  return -1;
}

void SlippiMatchmaking::MatchmakeThread()
{
  while (IsSearching())
  {
    if (is_mm_terminated)
    {
      break;
    }

    switch (m_state)
    {
    case ProcessState::INITIALIZING:
      startMatchmaking();
      break;
    case ProcessState::MATCHMAKING:
      handleMatchmaking();
      break;
    case ProcessState::OPPONENT_CONNECTING:
      handleConnecting();
      break;
    }
  }

  // Clean up ENET connections
  terminateMmConnection();
}

void SlippiMatchmaking::disconnectFromServer()
{
  is_mm_connected = false;

  if (m_server)
    enet_peer_disconnect(m_server, 0);
  else
    return;

  ENetEvent net_event;
  while (enet_host_service(m_client, &net_event, 3000) > 0)
  {
    switch (net_event.type)
    {
    case ENET_EVENT_TYPE_RECEIVE:
      enet_packet_destroy(net_event.packet);
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

void SlippiMatchmaking::terminateMmConnection()
{
  // Disconnect from server
  disconnectFromServer();

  // Destroy client
  if (m_client)
  {
    enet_host_destroy(m_client);
    m_client = nullptr;
  }
}

// Fallback: arbitrarily choose the last available local IP address listed. They seem to be listed
// in decreasing order IE. 192.168.0.100 > 192.168.0.10 > 10.0.0.2
// SLIPPITODO: refactor to use getaddrinfo since it is now the standard
static char* getLocalAddressFallback()
{
  char host[256];
  int hostname = gethostname(host, sizeof(host));  // find the host name
  if (hostname == -1)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Error finding LAN address");
    return nullptr;
  }

  struct hostent* host_entry = gethostbyname(host);  // find host information
  if (host_entry == NULL || host_entry->h_addrtype != AF_INET || host_entry->h_addr_list[0] == 0)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Error finding LAN host");
    return nullptr;
  }

  // Fetch the last IP (because that was correct for me, not sure if it will be for all)
  for (int i = 0; host_entry->h_addr_list[i] != 0; i++)
    if (host_entry->h_addr_list[i + 1] == 0)
      return host_entry->h_addr_list[i];
  return nullptr;
}

// Set up and connect a socket (UDP, so "connect" doesn't actually send any
// packets) so that the OS will determine what device/local IP address we will
// actually use.
static enet_uint32 getLocalAddress(ENetAddress* mm_address)
{
  ENetSocket socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (socket == -1)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Failed to get local address: socket create");
    enet_socket_destroy(socket);
    return 0;
  }

  if (enet_socket_connect(socket, mm_address) == -1)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Failed to get local address: socket connect");
    enet_socket_destroy(socket);
    return 0;
  }

  ENetAddress enet_address;
  if (enet_socket_get_address(socket, &enet_address) == -1)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Failed to get local address: socket get address");
    enet_socket_destroy(socket);
    return 0;
  }

  enet_socket_destroy(socket);
  return enet_address.host;
}

void SlippiMatchmaking::startMatchmaking()
{
  // I don't understand why I have to do this... if I don't do this, rand always returns the
  // same value
  m_client = nullptr;

  int retry_count = 0;
  auto user_info = m_user->GetUserInfo();
  while (m_client == nullptr && retry_count < 15)
  {
    if (Config::Get(Config::SLIPPI_FORCE_NETPLAY_PORT))
      m_host_port = Config::Get(Config::SLIPPI_NETPLAY_PORT);
    else
      m_host_port = 41000 + (generator() % 10000);
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Port to use: {}...", m_host_port);

    // We are explicitly setting the client address because we are trying to utilize our connection
    // to the matchmaking service in order to hole punch. This port will end up being the port
    // we listen on when we start our server
    ENetAddress client_addr;
    client_addr.host = ENET_HOST_ANY;
    client_addr.port = m_host_port;

    m_client = enet_host_create(&client_addr, 1, 3, 0, 0);
    retry_count++;
  }

  if (m_client == nullptr)
  {
    // Failed to create client
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_error_msg = "Failed to create mm client";
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Failed to create client...");
    return;
  }

  ENetAddress addr;
  enet_address_set_host(&addr, MM_HOST.c_str());
  addr.port = MM_PORT;

  m_server = enet_host_connect(m_client, &addr, 3, 0);

  if (m_server == nullptr)
  {
    // Failed to connect to server
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_error_msg = "Failed to start connection to mm server";
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Failed to start connection to mm server...");
    return;
  }

  // Before we can request a ticket, we must wait for connection to be successful
  int connect_attempt_count = 0;
  while (!is_mm_connected)
  {
    ENetEvent net_event;
    int net = enet_host_service(m_client, &net_event, 500);
    if (net <= 0 || net_event.type != ENET_EVENT_TYPE_CONNECT)
    {
      // Not yet connected, will retry
      connect_attempt_count++;
      if (connect_attempt_count >= 20)
      {
        ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Failed to connect to mm server...");
        m_state = ProcessState::ERROR_ENCOUNTERED;
        m_error_msg = "Failed to connect to mm server";
        return;
      }

      continue;
    }

    net_event.peer->data = &user_info.display_name;
    m_client->intercept = Common::ENet::InterceptCallback;
    is_mm_connected = true;
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Connected to mm server...");
  }

  ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Trying to find match...");

  /*if (!m_user->IsLoggedIn())
  {
      ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Must be logged in to queue");
      m_state = ProcessState::ERROR_ENCOUNTERED;
      m_error_msg = "Must be logged in to queue. Go back to menu";
      return;
  }*/

  // Determine local IP address. We can attempt to connect to our opponent via
  // local IP address if we have the same external IP address. The following
  // scenarios can cause us to have the same external IP address:
  // - we are connected to the same LAN
  // - we are connected to the same VPN node
  // - we are behind the same CGNAT
  char lan_addr[30]{};

  // attempt at using getaddrinfo, only ever sees 127.0.0.1 for some reason. for now the existing
  // impl will work because we only use ipv4.
  // struct addrinfo hints = {0}, *addrs;
  // hints.ai_family = AF_INET;
  // const int status = getaddrinfo(nullptr, std::to_string(m_host_port).c_str(), &hints, &addrs);
  // if (status != 0)
  // {
  //   ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Error finding LAN address");
  //   return;
  // }
  // for (struct addrinfo* addrinfo = addrs; addrinfo != nullptr; addrinfo = addrinfo->ai_next)
  // {
  //   if (getnameinfo(addrs->ai_addr, static_cast<socklen_t>(addrs->ai_addrlen), lan_addr,
  //                   sizeof(lan_addr), nullptr, 0, NI_NUMERICHOST))
  //   {
  //     continue;
  //   }
  //   WARN_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] IP via getaddrinfo {}", lan_addr);
  // }
  // freeaddrinfo(addrs);

  if (Config::Get(Config::SLIPPI_FORCE_LAN_IP))
  {
    WARN_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Overwriting LAN IP sent with configured address");
    sprintf(lan_addr, "%s:%d", Config::Get(Config::SLIPPI_LAN_IP).c_str(), m_host_port);
  }
  else
  {
    enet_uint32 local_address = getLocalAddress(&addr);
    if (local_address != 0)
    {
      sprintf(lan_addr, "%s:%d", inet_ntoa(*(struct in_addr*)&local_address), m_host_port);
    }
    else
    {
      char* fallback_address = getLocalAddressFallback();
      if (fallback_address != nullptr)
      {
        sprintf(lan_addr, "%s:%d", inet_ntoa(*(struct in_addr*)fallback_address), m_host_port);
      }
    }
  }

  WARN_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Sending LAN address: {}", lan_addr);

  std::vector<u8> connect_code_buf;
  connect_code_buf.insert(connect_code_buf.end(), m_search_settings.connect_code.begin(),
                          m_search_settings.connect_code.end());

  // Send message to server to create ticket
  json request;
  request["type"] = MmMessageType::CREATE_TICKET;
  request["user"] = {{"uid", user_info.uid},
                     {"playKey", user_info.play_key},
                     {"connectCode", user_info.connect_code},
                     {"displayName", user_info.display_name}};
  request["search"] = {{"mode", m_search_settings.mode}, {"connectCode", connect_code_buf}};
  request["appVersion"] = Common::GetSemVerStr();
  request["ipAddressLan"] = lan_addr;
  sendMessage(request);

  // Get response from server
  json response;
  int rcv_res = receiveMessage(response, 5000);
  if (rcv_res != 0)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE,
                  "[Matchmaking] Did not receive response from server for create ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_error_msg = "Failed to join mm queue";
    return;
  }

  std::string resp_type = response["type"];
  if (resp_type != MmMessageType::CREATE_TICKET_RESP)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Received incorrect response for create ticket");
    ERROR_LOG_FMT(SLIPPI_ONLINE, "{}", response.dump().c_str());
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_error_msg = "Invalid response when joining mm queue";
    return;
  }

  std::string err = response.value("error", "");
  if (err.length() > 0)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Received error from server for create ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_error_msg = err;
    return;
  }

  m_state = ProcessState::MATCHMAKING;
  ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Request ticket success");
}

void SlippiMatchmaking::handleMatchmaking()
{
  // Deal with class shut down
  if (m_state != ProcessState::MATCHMAKING)
    return;

  // Get response from server
  json get_resp;
  int rcv_res = receiveMessage(get_resp, 2000);
  if (rcv_res == -1)
  {
    INFO_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Have not yet received assignment");
    return;
  }
  else if (rcv_res != 0)
  {
    // Right now the only other code is -2 meaning the server died probably?
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Lost connection to the mm server");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_error_msg = "Lost connection to the mm server";
    return;
  }

  std::string resp_type = get_resp["type"];
  if (resp_type != MmMessageType::GET_TICKET_RESP)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Received incorrect response for get ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_error_msg = "Invalid response when getting mm status";
    return;
  }

  std::string err = get_resp.value("error", "");
  std::string latestVersion = get_resp.value("latestVersion", "");
  if (err.length() > 0)
  {
    if (latestVersion != "")
    {
      // Update version number when the mm server tells us our version is outdated
      // for people whose file updates dont work
      m_user->OverwriteLatestVersion(latestVersion);
    }

    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Received error from server for get ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_error_msg = err;
    return;
  }

  m_is_swap_attempt = false;
  m_netplay_client = nullptr;

  // Clear old users
  m_remote_ips.clear();
  m_player_info.clear();

  std::string match_id = get_resp.value("matchId", "");
  WARN_LOG_FMT(SLIPPI_ONLINE, "Match ID: {}", match_id);

  auto queue = get_resp["players"];
  if (queue.is_array())
  {
    std::string local_external_ip = "";

    for (json::iterator it = queue.begin(); it != queue.end(); ++it)
    {
      json el = *it;
      SlippiUser::UserInfo player_info;

      bool is_local = el.value("isLocalPlayer", false);
      player_info.uid = el.value("uid", "");
      player_info.display_name = el.value("displayName", "");
      player_info.connect_code = el.value("connectCode", "");
      player_info.port = el.value("port", 0);
      if (el["chatMessages"].is_array())
      {
        player_info.chat_messages = el.value("chatMessages", m_user->GetDefaultChatMessages());
        if (player_info.chat_messages.size() != 16)
        {
          player_info.chat_messages = m_user->GetDefaultChatMessages();
        }
      }
      else
      {
        player_info.chat_messages = m_user->GetDefaultChatMessages();
      }

      m_player_info.push_back(player_info);

      if (is_local)
      {
        std::vector<std::string> local_ip_parts;
        local_ip_parts = SplitString(el.value("ipAddress", "1.1.1.1:123"), ':');
        local_external_ip = local_ip_parts[0];
        m_local_player_idx = player_info.port - 1;
      }
    };

    // Loop a second time to get the correct remote IPs
    for (json::iterator it = queue.begin(); it != queue.end(); ++it)
    {
      json el = *it;

      if (el.value("port", 0) - 1 == m_local_player_idx)
        continue;

      auto ext_ip = el.value("ipAddress", "1.1.1.1:123");
      std::vector<std::string> ext_ip_parts = SplitString(ext_ip, ':');

      auto lanIp = el.value("ipAddressLan", "1.1.1.1:123");

      WARN_LOG_FMT(SLIPPI_ONLINE, "LAN IP: {}", lanIp.c_str());

      if (ext_ip_parts[0] != local_external_ip || lanIp.empty())
      {
        // If external IPs are different, just use that address
        m_remote_ips.push_back(ext_ip);
        continue;
      }

      // TODO: Instead of using one or the other, it might be better to try both

      // If external IPs are the same, try using LAN IPs
      m_remote_ips.push_back(lanIp);
    }
  }
  m_is_host = get_resp.value("isHost", false);

  // Get allowed stages. For stage select modes like direct and teams, this will only impact the
  // first map selected
  m_allowed_stages.clear();
  auto stages = get_resp["stages"];
  if (stages.is_array())
  {
    for (json::iterator it = stages.begin(); it != stages.end(); ++it)
    {
      json el = *it;
      auto stage_id = el.get<int>();
      m_allowed_stages.push_back(stage_id);
    }
  }

  if (m_allowed_stages.empty())
  {
    // Default case, shouldn't ever really be hit but it's here just in case
    m_allowed_stages.push_back(0x3);   // Pokemon
    m_allowed_stages.push_back(0x8);   // Yoshi's Story
    m_allowed_stages.push_back(0x1C);  // Dream Land
    m_allowed_stages.push_back(0x1F);  // Battlefield
    m_allowed_stages.push_back(0x20);  // Final Destination

    // Add FoD if singles
    if (m_player_info.size() == 2)
    {
      m_allowed_stages.push_back(0x2);  // FoD
    }
  }

  m_mm_result.id = match_id;
  m_mm_result.players = m_player_info;
  m_mm_result.stages = m_allowed_stages;

  // Disconnect and destroy enet client to mm server
  terminateMmConnection();

  m_state = ProcessState::OPPONENT_CONNECTING;
  ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Opponent found. is_decider: {}",
                m_is_host ? "true" : "false");
}

int SlippiMatchmaking::LocalPlayerIndex()
{
  return m_local_player_idx;
}

std::vector<SlippiUser::UserInfo> SlippiMatchmaking::GetPlayerInfo()
{
  return m_player_info;
}

std::vector<u16> SlippiMatchmaking::GetStages()
{
  return m_allowed_stages;
}

std::string SlippiMatchmaking::GetPlayerName(u8 port)
{
  if (port >= m_player_info.size())
  {
    return "";
  }
  return m_player_info[port].display_name;
}

SlippiMatchmaking::MatchmakeResult SlippiMatchmaking::GetMatchmakeResult()
{
  return m_mm_result;
}

u8 SlippiMatchmaking::RemotePlayerCount()
{
  if (m_player_info.size() == 0)
    return 0;

  return (u8)m_player_info.size() - 1;
}

void SlippiMatchmaking::handleConnecting()
{
  auto user_info = m_user->GetUserInfo();

  m_is_swap_attempt = false;
  m_netplay_client = nullptr;

  u8 remote_player_count = (u8)m_remote_ips.size();
  std::vector<std::string> remote_parts;
  std::vector<std::string> addrs;
  std::vector<u16> ports;
  for (int i = 0; i < m_remote_ips.size(); i++)
  {
    remote_parts.clear();
    remote_parts = SplitString(m_remote_ips[i], ':');
    addrs.push_back(remote_parts[0]);
    ports.push_back(std::stoi(remote_parts[1]));
  }

  std::stringstream ip_log;
  ip_log << "Remote player IPs: ";
  for (int i = 0; i < m_remote_ips.size(); i++)
  {
    ip_log << m_remote_ips[i] << ", ";
  }
  // INFO_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] My port: {} || {}", m_host_port, ip_log.str());

  // Is host is now used to specify who the decider is
  auto client = std::make_unique<SlippiNetplayClient>(addrs, ports, remote_player_count,
                                                      m_host_port, m_is_host, m_local_player_idx);

  while (!m_netplay_client)
  {
    auto status = client->GetSlippiConnectStatus();
    if (status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED)
    {
      INFO_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Connection not yet successful");
      Common::SleepCurrentThread(500);

      // Deal with class shut down
      if (m_state != ProcessState::OPPONENT_CONNECTING)
        return;

      continue;
    }
    else if (status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_FAILED &&
             m_search_settings.mode == SlippiMatchmaking::OnlinePlayMode::TEAMS)
    {
      // If we failed setting up a connection in teams mode, show a detailed error about who we had
      // issues connecting to.
      ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Failed to connect to players");
      m_state = ProcessState::ERROR_ENCOUNTERED;
      m_error_msg = "Timed out waiting for other players to connect";
      auto failed_conns = client->GetFailedConnections();
      if (!failed_conns.empty())
      {
        std::stringstream err;
        err << "Could not connect to players: ";
        for (int i = 0; i < failed_conns.size(); i++)
        {
          int p = failed_conns[i];
          if (p >= m_local_player_idx)
            p++;

          err << m_player_info[p].display_name;
          if (i < failed_conns.size() - 1)
            err << ", ";
        }
        m_error_msg = err.str();
      }

      return;
    }
    else if (status != SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED)
    {
      ERROR_LOG_FMT(SLIPPI_ONLINE,
                    "[Matchmaking] Connection attempt failed, looking for someone else.");

      // Return to the start to get a new ticket to find someone else we can hopefully connect with
      m_netplay_client = nullptr;
      m_state = ProcessState::INITIALIZING;
      return;
    }

    ERROR_LOG_FMT(SLIPPI_ONLINE, "[Matchmaking] Connection success!");

    // Successful connection
    m_netplay_client = std::move(client);
  }

  // Connection success, our work is done
  m_state = ProcessState::CONNECTION_SUCCESS;
}
