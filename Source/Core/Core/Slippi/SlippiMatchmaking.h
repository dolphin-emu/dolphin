#pragma once

#include "Common/CommonTypes.h"
#include "Common/Thread.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiUser.h"

#include <random>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <nlohmann/json.hpp>
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
    TEAMS = 3,
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

  struct MatchmakeResult
  {
    std::string id = "";
    std::vector<SlippiUser::UserInfo> players;
    std::vector<u16> stages;
  };

  void FindMatch(MatchSearchSettings settings);
  void MatchmakeThread();
  ProcessState GetMatchmakeState();
  bool IsSearching();
  std::unique_ptr<SlippiNetplayClient> GetNetplayClient();
  std::string GetErrorMessage();
  int LocalPlayerIndex();
  std::vector<SlippiUser::UserInfo> GetPlayerInfo();
  std::string GetPlayerName(u8 port);
  std::vector<u16> GetStages();
  u8 RemotePlayerCount();
  MatchmakeResult GetMatchmakeResult();
  static bool IsFixedRulesMode(OnlinePlayMode mode);

protected:
  const std::string MM_HOST_DEV = "mm2.slippi.gg";  // Dev host
  const std::string MM_HOST_PROD = "mm.slippi.gg";  // Production host
  const u16 MM_PORT = 43113;

  std::string MM_HOST = "";

  ENetHost* m_client;
  ENetPeer* m_server;

  std::default_random_engine generator;

  bool isMmConnected = false;
  bool is_mm_terminated = false;

  std::thread m_matchmakeThread;

  MatchSearchSettings m_searchSettings;

  ProcessState m_state;
  std::string m_errorMsg = "";

  SlippiUser* m_user;

  int m_isSwapAttempt = false;

  int m_hostPort;
  int m_localPlayerIndex;
  std::vector<std::string> m_remoteIps;
  MatchmakeResult m_mm_result;
  std::vector<SlippiUser::UserInfo> m_playerInfo;
  std::vector<u16> m_allowedStages;
  bool m_joinedLobby;
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

  void startMatchmaking();
  void handleMatchmaking();
  void handleConnecting();
};
