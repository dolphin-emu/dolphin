#pragma once

#include "Common/CommonTypes.h"
#include "Common/Thread.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiUser.h"

#include <enet/enet.h>
#include <unordered_map>
#include <vector>

#include <json.hpp>
using json = nlohmann::json;

class SlippiMatchmaking
{
public:
  SlippiMatchmaking(SlippiUser* user);
  ~SlippiMatchmaking();

  enum OnlinePlayMode
  {
    RANKED = 0,
    UNRANKED = 1,
    DIRECT = 2,
  };

  enum ProcessState
  {
    IDLE,
    INITIALIZING,
    MATCHMAKING,
    OPPONENT_CONNECTING,
    CONNECTION_SUCCESS,
    ERROR_ENCOUNTERED,
  };

  struct MatchSearchSettings
  {
    OnlinePlayMode mode = OnlinePlayMode::RANKED;
    std::string connectCode = "";
  };

  void FindMatch(MatchSearchSettings settings);
  void MatchmakeThread();
  ProcessState GetMatchmakeState();
  bool IsSearching();
  std::unique_ptr<SlippiNetplayClient> GetNetplayClient();
  std::string GetErrorMessage();

protected:
  const std::string MM_HOST_DEV = "35.197.121.196"; // Dev host
  const std::string MM_HOST_PROD = "35.247.98.48";  // Production host
  const u16 MM_PORT = 43113;

  std::string MM_HOST = "";

  ENetHost* m_client;
  ENetPeer* m_server;

  std::default_random_engine generator;

  bool isMmConnected = false;

  std::thread m_matchmakeThread;

  MatchSearchSettings m_searchSettings;

  ProcessState m_state;
  std::string m_errorMsg = "";

  SlippiUser* m_user;

  int m_isSwapAttempt = false;

  int m_hostPort;
  std::string m_oppIp;
  bool m_isHost;

  std::unique_ptr<SlippiNetplayClient> m_netplayClient;

  const std::unordered_map<ProcessState, bool> searchingStates = {
      {ProcessState::INITIALIZING, true},
      {ProcessState::MATCHMAKING, true},
      {ProcessState::OPPONENT_CONNECTING, true},
  };

  void disconnectFromServer();
  void terminateMmConnection();
  void sendMessage(json msg);
  int receiveMessage(json& msg, int maxAttempts);

  void sendHolePunchMsg(std::string remoteIp, u16 remotePort, u16 localPort);

  void startMatchmaking();
  void handleMatchmaking();
  void handleConnecting();
};
