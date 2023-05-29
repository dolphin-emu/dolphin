// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <array>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include <rcheevos/include/rc_runtime.h>

#include "Common/Event.h"
#include "Common/WorkQueueThread.h"

using AchievementId = u32;
constexpr size_t RP_SIZE = 256;
using RichPresence = std::array<char, RP_SIZE>;

namespace Core
{
class System;
}

class AchievementManager
{
public:
  enum class ResponseType
  {
    SUCCESS,
    MANAGER_NOT_INITIALIZED,
    INVALID_REQUEST,
    INVALID_CREDENTIALS,
    CONNECTION_FAILED,
    UNKNOWN_FAILURE
  };
  using ResponseCallback = std::function<void(ResponseType)>;

  struct PointSpread
  {
    u32 total_count;
    u32 total_points;
    u32 hard_unlocks;
    u32 hard_points;
    u32 soft_unlocks;
    u32 soft_points;
  };

  static AchievementManager* GetInstance();
  void Init();
  ResponseType Login(const std::string& password);
  void LoginAsync(const std::string& password, const ResponseCallback& callback);
  bool IsLoggedIn() const;
  void LoadGameByFilenameAsync(const std::string& iso_path, const ResponseCallback& callback);

  void LoadUnlockData(const ResponseCallback& callback);
  void ActivateDeactivateAchievements();
  void ActivateDeactivateLeaderboards();
  void ActivateDeactivateRichPresence();

  void DoFrame();
  u32 MemoryPeeker(u32 address, u32 num_bytes, void* ud);
  void AchievementEventHandler(const rc_runtime_event_t* runtime_event);

  void CloseGame();
  void Logout();
  void Shutdown();

private:
  AchievementManager() = default;

  static constexpr int HASH_LENGTH = 33;

  ResponseType VerifyCredentials(const std::string& password);
  ResponseType ResolveHash(std::array<char, HASH_LENGTH> game_hash);
  ResponseType StartRASession();
  ResponseType FetchGameData();
  ResponseType FetchUnlockData(bool hardcore);

  void ActivateDeactivateAchievement(AchievementId id, bool enabled, bool unofficial, bool encore);
  RichPresence GenerateRichPresence();

  ResponseType AwardAchievement(AchievementId achievement_id);
  ResponseType SubmitLeaderboard(AchievementId leaderboard_id, int value);
  ResponseType PingRichPresence(const RichPresence& rich_presence);

  void HandleAchievementTriggeredEvent(const rc_runtime_event_t* runtime_event);
  void HandleLeaderboardStartedEvent(const rc_runtime_event_t* runtime_event);
  void HandleLeaderboardCanceledEvent(const rc_runtime_event_t* runtime_event);
  void HandleLeaderboardTriggeredEvent(const rc_runtime_event_t* runtime_event);

  PointSpread TallyScore() const;

  template <typename RcRequest, typename RcResponse>
  ResponseType Request(RcRequest rc_request, RcResponse* rc_response,
                       const std::function<int(rc_api_request_t*, const RcRequest*)>& init_request,
                       const std::function<int(RcResponse*, const char*)>& process_response);

  rc_runtime_t m_runtime{};
  Core::System* m_system{};
  bool m_is_runtime_initialized = false;
  std::string m_display_name;
  std::array<char, HASH_LENGTH> m_game_hash{};
  u32 m_game_id = 0;
  rc_api_fetch_game_data_response_t m_game_data{};
  bool m_is_game_loaded = false;
  u64 m_last_ping_time = 0;

  struct UnlockStatus
  {
    AchievementId game_data_index = 0;
    enum class UnlockType
    {
      LOCKED,
      SOFTCORE,
      HARDCORE
    } remote_unlock_status = UnlockType::LOCKED;
    u32 session_unlock_count = 0;
    u32 points = 0;
  };
  std::unordered_map<AchievementId, UnlockStatus> m_unlock_map;

  Common::WorkQueueThread<std::function<void()>> m_queue;
  std::recursive_mutex m_lock;
};  // class AchievementManager

#endif  // USE_RETRO_ACHIEVEMENTS
