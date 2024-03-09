// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <array>
#include <ctime>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include <rcheevos/include/rc_client.h>
#include <rcheevos/include/rc_runtime.h>

#include "Common/Event.h"
#include "Common/HttpRequest.h"
#include "Common/WorkQueueThread.h"
#include "DiscIO/Volume.h"

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

namespace OSD
{
struct Icon;
}

class AchievementManager
{
public:
  enum class ResponseType
  {
    SUCCESS,
    NOT_ENABLED,
    MANAGER_NOT_INITIALIZED,
    INVALID_REQUEST,
    INVALID_CREDENTIALS,
    CONNECTION_FAILED,
    MALFORMED_OBJECT,
    EXPIRED_CONTEXT,
    UNKNOWN_FAILURE
  };
  using ResponseCallback = std::function<void(ResponseType)>;
  using UpdateCallback = std::function<void()>;
  using BadgeNameFunction = std::function<std::string(const AchievementManager&)>;

  struct PointSpread
  {
    u32 total_count;
    u32 total_points;
    u32 hard_unlocks;
    u32 hard_points;
    u32 soft_unlocks;
    u32 soft_points;
  };

  static constexpr size_t HASH_SIZE = 33;
  using Hash = std::array<char, HASH_SIZE>;
  using AchievementId = u32;
  static constexpr size_t FORMAT_SIZE = 24;
  using FormattedValue = std::array<char, FORMAT_SIZE>;
  using LeaderboardRank = u32;
  static constexpr size_t RP_SIZE = 256;
  using RichPresence = std::array<char, RP_SIZE>;
  using Badge = std::vector<u8>;
  using NamedIconMap = std::map<std::string, std::unique_ptr<OSD::Icon>, std::less<>>;
  static constexpr size_t MAX_DISPLAYED_LBOARDS = 4;

  struct BadgeStatus
  {
    std::string name = "";
    Badge badge{};
  };

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
    BadgeStatus locked_badge;
    BadgeStatus unlocked_badge;
    u32 category = RC_ACHIEVEMENT_CATEGORY_CORE;
  };

  static constexpr std::string_view GRAY = "transparent";
  static constexpr std::string_view GOLD = "#FFD700";
  static constexpr std::string_view BLUE = "#0B71C1";

  struct LeaderboardEntry
  {
    std::string username;
    FormattedValue score;
    LeaderboardRank rank;
  };

  struct LeaderboardStatus
  {
    std::string name;
    std::string description;
    u32 player_index = 0;
    std::unordered_map<u32, LeaderboardEntry> entries;
  };

  static AchievementManager& GetInstance();
  void Init();
  void SetUpdateCallback(UpdateCallback callback);
  void Login(const std::string& password);
  bool HasAPIToken() const;
  void LoadGame(const std::string& file_path, const DiscIO::Volume* volume);
  bool IsGameLoaded() const;

  void FetchPlayerBadge();
  void FetchGameBadges();

  void DoFrame();

  std::recursive_mutex& GetLock();
  bool IsHardcoreModeActive() const;
  std::string_view GetPlayerDisplayName() const;
  u32 GetPlayerScore() const;
  const BadgeStatus& GetPlayerBadge() const;
  std::string_view GetGameDisplayName() const;
  PointSpread TallyScore() const;
  rc_client_t* GetClient();
  rc_api_fetch_game_data_response_t* GetGameData();
  const BadgeStatus& GetGameBadge() const;
  const BadgeStatus& GetAchievementBadge(AchievementId id, bool locked) const;
  const UnlockStatus* GetUnlockStatus(AchievementId achievement_id) const;
  AchievementManager::ResponseType GetAchievementProgress(AchievementId achievement_id, u32* value,
                                                          u32* target);
  const std::unordered_map<AchievementId, LeaderboardStatus>& GetLeaderboardsInfo() const;
  RichPresence GetRichPresence() const;
  bool IsDisabled() const { return m_disabled; };
  void SetDisabled(bool disabled);
  const NamedIconMap& GetChallengeIcons() const;
  std::vector<std::string> GetActiveLeaderboards() const;

  void DoState(PointerWrap& p);

  void CloseGame();
  void Logout();
  void Shutdown();

private:
  AchievementManager() = default;

  struct FilereaderState
  {
    int64_t position = 0;
    std::unique_ptr<DiscIO::Volume> volume;
  };

  const BadgeStatus m_default_badge;

  static void* FilereaderOpenByFilepath(const char* path_utf8);
  static void* FilereaderOpenByVolume(const char* path_utf8);
  static void FilereaderSeek(void* file_handle, int64_t offset, int origin);
  static int64_t FilereaderTell(void* file_handle);
  static size_t FilereaderRead(void* file_handle, void* buffer, size_t requested_bytes);
  static void FilereaderClose(void* file_handle);

  static void LoginCallback(int result, const char* error_message, rc_client_t* client,
                            void* userdata);

  ResponseType FetchBoardInfo(AchievementId leaderboard_id);

  std::unique_ptr<DiscIO::Volume>& GetLoadingVolume() { return m_loading_volume; };

  void GenerateRichPresence(const Core::CPUThreadGuard& guard);

  ResponseType PingRichPresence(const RichPresence& rich_presence);

  static void LoadGameCallback(int result, const char* error_message, rc_client_t* client,
                               void* userdata);
  void DisplayWelcomeMessage();

  static void HandleAchievementTriggeredEvent(const rc_client_event_t* client_event);
  static void HandleLeaderboardStartedEvent(const rc_client_event_t* client_event);
  static void HandleLeaderboardFailedEvent(const rc_client_event_t* client_event);
  static void HandleLeaderboardSubmittedEvent(const rc_client_event_t* client_event);
  static void HandleLeaderboardTrackerUpdateEvent(const rc_client_event_t* client_event);
  static void HandleLeaderboardTrackerShowEvent(const rc_client_event_t* client_event);
  static void HandleLeaderboardTrackerHideEvent(const rc_client_event_t* client_event);
  static void HandleAchievementChallengeIndicatorShowEvent(const rc_client_event_t* client_event);
  static void HandleAchievementChallengeIndicatorHideEvent(const rc_client_event_t* client_event);
  static void HandleAchievementProgressIndicatorShowEvent(const rc_client_event_t* client_event);
  static void HandleGameCompletedEvent(const rc_client_event_t* client_event, rc_client_t* client);

  template <typename RcRequest, typename RcResponse>
  ResponseType Request(RcRequest rc_request, RcResponse* rc_response,
                       const std::function<int(rc_api_request_t*, const RcRequest*)>& init_request,
                       const std::function<int(RcResponse*, const char*)>& process_response);

  static void RequestV2(const rc_api_request_t* request, rc_client_server_callback_t callback,
                        void* callback_data, rc_client_t* client);
  static u32 MemoryPeeker(u32 address, u8* buffer, u32 num_bytes, rc_client_t* client);
  void FetchBadge(BadgeStatus* badge, u32 badge_type, const BadgeNameFunction function);
  static void EventHandler(const rc_client_event_t* event, rc_client_t* client);

  rc_runtime_t m_runtime{};
  rc_client_t* m_client{};
  Core::System* m_system{};
  bool m_is_runtime_initialized = false;
  UpdateCallback m_update_callback = [] {};
  std::unique_ptr<DiscIO::Volume> m_loading_volume;
  bool m_disabled = false;
  BadgeStatus m_player_badge;
  Hash m_game_hash{};
  u32 m_game_id = 0;
  rc_api_fetch_game_data_response_t m_game_data{};
  bool m_is_game_loaded = false;
  u32 m_framecount = 0;
  BadgeStatus m_game_badge;
  std::unordered_map<AchievementId, BadgeStatus> m_unlocked_badges;
  std::unordered_map<AchievementId, BadgeStatus> m_locked_badges;
  RichPresence m_rich_presence;
  time_t m_last_ping_time = 0;

  std::unordered_map<AchievementId, UnlockStatus> m_unlock_map;
  std::unordered_map<AchievementId, LeaderboardStatus> m_leaderboard_map;
  NamedIconMap m_active_challenges;
  std::vector<rc_client_leaderboard_tracker_t> m_active_leaderboards;

  Common::WorkQueueThread<std::function<void()>> m_queue;
  Common::WorkQueueThread<std::function<void()>> m_image_queue;
  mutable std::recursive_mutex m_lock;
  std::recursive_mutex m_filereader_lock;
};  // class AchievementManager

#endif  // USE_RETRO_ACHIEVEMENTS
