#include "SlippiMatchmaking.h"
#include "Common/Common.h"
#include "Common/Version.h"
#include "Common/ENetUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include <string>
#include <vector>

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
  m_errorMsg = "";

  m_client = nullptr;
  m_server = nullptr;

  MM_HOST = Common::scm_slippi_semver_str.find("dev") == std::string::npos ? MM_HOST_PROD : MM_HOST_DEV;

  generator = std::default_random_engine(Common::Timer::GetTimeMs());
}

SlippiMatchmaking::~SlippiMatchmaking()
{
  m_state = ProcessState::ERROR_ENCOUNTERED;
  m_errorMsg = "Matchmaking shut down";

  if (m_matchmakeThread.joinable())
    m_matchmakeThread.join();

  terminateMmConnection();
}

void SlippiMatchmaking::FindMatch(MatchSearchSettings settings)
{
  isMmConnected = false;

  ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Starting matchmaking...");

  m_searchSettings = settings;

  m_errorMsg = "";
  m_state = ProcessState::INITIALIZING;
  m_matchmakeThread = std::thread(&SlippiMatchmaking::MatchmakeThread, this);
}

SlippiMatchmaking::ProcessState SlippiMatchmaking::GetMatchmakeState()
{
  return m_state;
}

std::string SlippiMatchmaking::GetErrorMessage()
{
  return m_errorMsg;
}

SlippiUser::UserInfo SlippiMatchmaking::GetOpponent()
{
  return m_oppUser;
}

bool SlippiMatchmaking::IsSearching()
{
  return searchingStates.count(m_state) != 0;
}

std::unique_ptr<SlippiNetplayClient> SlippiMatchmaking::GetNetplayClient()
{
  return std::move(m_netplayClient);
}

void SlippiMatchmaking::sendMessage(json msg)
{
  enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
  u8 channelId = 0;

  std::string msgContents = msg.dump();

  ENetPacket* epac = enet_packet_create(msgContents.c_str(), msgContents.length(), flags);
  enet_peer_send(m_server, channelId, epac);
}

int SlippiMatchmaking::receiveMessage(json& msg, int timeoutMs)
{
  int hostServiceTimeoutMs = 250;

  // Make sure loop runs at least once
  if (timeoutMs < hostServiceTimeoutMs)
    timeoutMs = hostServiceTimeoutMs;

  // This is not a perfect way to timeout but hopefully it's close enough?
  int maxAttempts = timeoutMs / hostServiceTimeoutMs;

  for (int i = 0; i < maxAttempts; i++)
  {
    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, hostServiceTimeoutMs);
    if (net <= 0)
      continue;

    switch (netEvent.type)
    {
    case ENET_EVENT_TYPE_RECEIVE:
    {
      std::vector<u8> buf;
      buf.insert(buf.end(), netEvent.packet->data, netEvent.packet->data + netEvent.packet->dataLength);

      std::string str(buf.begin(), buf.end());
      INFO_LOG(SLIPPI_ONLINE, "[Matchmaking] Received: %s", str.c_str());
      msg = json::parse(str);

      enet_packet_destroy(netEvent.packet);
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
  isMmConnected = false;

  if (m_server)
    enet_peer_disconnect(m_server, 0);
  else
    return;

  ENetEvent netEvent;
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

void SlippiMatchmaking::startMatchmaking()
{
  // I don't understand why I have to do this... if I don't do this, rand always returns the
  // same value
  m_client = nullptr;

  int retryCount = 0;
  while (m_client == nullptr && retryCount < 15)
  {
    m_hostPort = 49000 + (generator() % 2000);
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Port to use: %d...", m_hostPort);

    // We are explicitly setting the client address because we are trying to utilize our connection
    // to the matchmaking service in order to hole punch. This port will end up being the port
    // we listen on when we start our server
    ENetAddress clientAddr;
    clientAddr.host = ENET_HOST_ANY;
    clientAddr.port = m_hostPort;

    m_client = enet_host_create(&clientAddr, 1, 3, 0, 0);
    retryCount++;
  }

  if (m_client == nullptr)
  {
    // Failed to create client
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Failed to create mm client";
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Failed to create client...");
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
    m_errorMsg = "Failed to start connection to mm server";
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Failed to start connection to mm server...");
    return;
  }

  // Before we can request a ticket, we must wait for connection to be successful
  int connectAttemptCount = 0;
  while (!isMmConnected)
  {
    ENetEvent netEvent;
    int net = enet_host_service(m_client, &netEvent, 500);
    if (net <= 0 || netEvent.type != ENET_EVENT_TYPE_CONNECT)
    {
      // Not yet connected, will retry
      connectAttemptCount++;
      if (connectAttemptCount >= 20)
      {
        ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Failed to connect to mm server...");
        m_state = ProcessState::ERROR_ENCOUNTERED;
        m_errorMsg = "Failed to connect to mm server";
        return;
      }

      continue;
    }

    m_client->intercept = ENetUtil::InterceptCallback;
    isMmConnected = true;
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Connected to mm server...");
  }

  ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Trying to find match...");

  if (!m_user->IsLoggedIn())
  {
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Must be logged in to queue");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Must be logged in to queue. Go back to menu";
    return;
  }

  auto userInfo = m_user->GetUserInfo();

  std::vector<u8> connectCodeBuf;
  connectCodeBuf.insert(connectCodeBuf.end(), m_searchSettings.connectCode.begin(),
    m_searchSettings.connectCode.end());

  // Send message to server to create ticket
  json request;
  request["type"] = MmMessageType::CREATE_TICKET;
  request["user"] = { {"uid", userInfo.uid}, {"playKey", userInfo.playKey} };
  request["search"] = { {"mode", m_searchSettings.mode}, {"connectCode", connectCodeBuf} };
  request["appVersion"] = Common::scm_slippi_semver_str;
  sendMessage(request);

  // Get response from server
  json response;
  int rcvRes = receiveMessage(response, 5000);
  if (rcvRes != 0)
  {
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Did not receive response from server for create ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Failed to join mm queue";
    return;
  }

  std::string respType = response["type"];
  if (respType != MmMessageType::CREATE_TICKET_RESP)
  {
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received incorrect response for create ticket");
    ERROR_LOG(SLIPPI_ONLINE, "%s", response.dump().c_str());
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Invalid response when joining mm queue";
    return;
  }

  std::string err = response.value("error", "");
  if (err.length() > 0)
  {
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received error from server for create ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = err;
    return;
  }

  m_state = ProcessState::MATCHMAKING;
  ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Request ticket success");
}

void SlippiMatchmaking::handleMatchmaking()
{
  // Deal with class shut down
  if (m_state != ProcessState::MATCHMAKING)
    return;

  // Get response from server
  json getResp;
  int rcvRes = receiveMessage(getResp, 2000);
  if (rcvRes == -1)
  {
    INFO_LOG(SLIPPI_ONLINE, "[Matchmaking] Have not yet received assignment");
    return;
  }
  else if (rcvRes != 0)
  {
    // Right now the only other code is -2 meaning the server died probably?
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Lost connection to the mm server");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Lost connection to the mm server";
    return;
  }

  std::string respType = getResp["type"];
  if (respType != MmMessageType::GET_TICKET_RESP)
  {
    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received incorrect response for get ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = "Invalid response when getting mm status";
    return;
  }

  std::string err = getResp.value("error", "");
  std::string latestVersion = getResp.value("latestVersion", "");
  if (err.length() > 0)
  {
    if (latestVersion != "")
    {
      // Update file to get new version number when the mm server tells us our version is outdated
      m_user->OverwriteLatestVersion(latestVersion); // Force latest version for people whose file updates dont work
    }

    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Received error from server for get ticket");
    m_state = ProcessState::ERROR_ENCOUNTERED;
    m_errorMsg = err;
    return;
  }

  m_isSwapAttempt = false;
  m_netplayClient = nullptr;
  m_oppIp = getResp.value("oppAddress", "");
  m_isHost = getResp.value("isHost", false);

  // Clear old user
  SlippiUser::UserInfo emptyInfo;
  m_oppUser = emptyInfo;

  auto oppUser = getResp["oppUser"];
  if (oppUser.is_object())
  {
    m_oppUser.uid = oppUser.value("uid", "");
    m_oppUser.displayName = oppUser.value("displayName", "");
    m_oppUser.connectCode = oppUser.value("connectCode", "");
  }

  // Disconnect and destroy enet client to mm server
  terminateMmConnection();

  m_state = ProcessState::OPPONENT_CONNECTING;
  ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Opponent found. isDecider: %s", m_isHost ? "true" : "false");
}

void SlippiMatchmaking::handleConnecting()
{
  std::vector<std::string> ipParts = SplitString(m_oppIp, ':');

  // Is host is now used to specify who the decider is
  auto client = std::make_unique<SlippiNetplayClient>(ipParts[0], std::stoi(ipParts[1]), m_hostPort, m_isHost);

  while (!m_netplayClient)
  {
    auto status = client->GetSlippiConnectStatus();
    if (status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_INITIATED)
    {
      INFO_LOG(SLIPPI_ONLINE, "[Matchmaking] Connection not yet successful");
      Common::SleepCurrentThread(500);

      // Deal with class shut down
      if (m_state != ProcessState::OPPONENT_CONNECTING)
        return;

      continue;
    }
    else if (status != SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED)
    {
      ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Connection attempt failed, looking for someone else.");

      // Return to the start to get a new ticket to find someone else we can hopefully connect with
      m_netplayClient = nullptr;
      m_state = ProcessState::INITIALIZING;
      return;
    }

    ERROR_LOG(SLIPPI_ONLINE, "[Matchmaking] Connection success!");

    // Successful connection
    m_netplayClient = std::move(client);
  }

  // Connection success, our work is done
  m_state = ProcessState::CONNECTION_SUCCESS;
}
