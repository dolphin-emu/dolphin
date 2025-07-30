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
  SlippiMatchmaking(uintptr_t rs_exi_device_ptr, SlippiUser* user);
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
    std::string connect_code = "";
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

  bool is_mm_connected = false;
  bool is_mm_terminated = false;

  std::thread m_matchmake_thread;

  MatchSearchSettings m_search_settings;

  ProcessState m_state;
  std::string m_error_msg = "";

  SlippiUser* m_user;

  int m_is_swap_attempt = false;

  int m_host_port;
  int m_local_player_idx;
  std::vector<std::string> m_remote_ips;
  MatchmakeResult m_mm_result;
  std::vector<SlippiUser::UserInfo> m_player_info;
  std::vector<u16> m_allowed_stages;
  bool m_joined_lobby;
  bool m_is_host;

  std::unique_ptr<SlippiNetplayClient> m_netplay_client;

  // A pointer to a "shadow" EXI Device that lives on the Rust side of things.
  // Do *not* do any cleanup of this! The EXI device will handle it.
  uintptr_t slprs_exi_device_ptr;

  const std::unordered_map<ProcessState, bool> searching_states = {
      {ProcessState::INITIALIZING, true},
      {ProcessState::MATCHMAKING, true},
      {ProcessState::OPPONENT_CONNECTING, true},
  };

  void disconnectFromServer();
  void terminateMmConnection();
  void sendMessage(json msg);
  int receiveMessage(json& msg, int max_attempts);

  void startMatchmaking();
  void handleMatchmaking();
  void handleConnecting();
};
