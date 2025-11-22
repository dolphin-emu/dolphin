// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <array>
#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include <rcheevos/include/rc_client.h>
#include <rcheevos/include/rc_hash.h>
#include <rcheevos/include/rc_runtime.h>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Event.h"
#include "Common/HookableEvent.h"
#include "Common/HttpRequest.h"
#include "Common/JsonUtil.h"
#include "Common/Lazy.h"
#include "Common/WorkQueueThread.h"
#include "DiscIO/Volume.h"
#include "VideoCommon/Assets/CustomTextureData.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
#include <rcheevos/include/rc_client_raintegration.h>
#endif  // RC_CLIENT_SUPPORTS_RAINTEGRATION

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

namespace PatchEngine
{
struct Patch;
}  // namespace PatchEngine

namespace Gecko
{
class GeckoCode;
}  // namespace Gecko

namespace ActionReplay
{
struct ARCode;
}  // namespace ActionReplay

class AchievementManager
{
public:
  using BadgeNameFunction = std::function<std::string(const AchievementManager&)>;

  static constexpr size_t HASH_SIZE = 33;
  using Hash = std::array<char, HASH_SIZE>;
  using AchievementId = u32;
  static constexpr size_t FORMAT_SIZE = 24;
  using FormattedValue = std::array<char, FORMAT_SIZE>;
  using LeaderboardRank = u32;
  static constexpr size_t RP_SIZE = 256;
  using RichPresence = std::array<char, RP_SIZE>;
  using Badge = VideoCommon::CustomTextureData::ArraySlice::Level;
  static constexpr size_t MAX_DISPLAYED_LBOARDS = 4;
  // This is hardcoded to 24MiB because rcheevos currently hardcodes it to 24MiB.
  static constexpr u32 MEM1_SIZE = 0x01800000;
  static constexpr u32 MEM2_START = 0x10000000;

  static constexpr std::string_view DEFAULT_PLAYER_BADGE_FILENAME = "achievements_player.png";
  static constexpr std::string_view DEFAULT_GAME_BADGE_FILENAME = "achievements_game.png";
  static constexpr std::string_view DEFAULT_LOCKED_BADGE_FILENAME = "achievements_locked.png";
  static constexpr std::string_view DEFAULT_UNLOCKED_BADGE_FILENAME = "achievements_unlocked.png";
  static constexpr std::string_view GRAY = "transparent";
  static constexpr std::string_view GOLD = "#FFD700";
  static constexpr std::string_view BLUE = "#0B71C1";
  static constexpr std::string_view APPROVED_LIST_FILENAME = "ApprovedInis.json";
  static const inline Common::SHA1::Digest APPROVED_LIST_HASH = {
      0xDF, 0x11, 0xD6, 0xA7, 0x2E, 0x8D, 0x3B, 0x3C, 0x41, 0x22,
      0x29, 0x3F, 0x67, 0x40, 0xD9, 0x92, 0xBF, 0xC0, 0x1C, 0x43};

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

  struct UpdatedItems
  {
    bool all = false;
    bool player_icon = false;
    bool game_icon = false;
    bool all_achievements = false;
    std::set<AchievementId> achievements{};
    bool all_leaderboards = false;
    std::set<AchievementId> leaderboards{};
    bool rich_presence = false;
    int failed_login_code = 0;
  };
  Common::HookableEvent<const UpdatedItems&> update_event;
  Common::HookableEvent<int> login_event;

  static AchievementManager& GetInstance();
  void Init(void* hwnd);
  void Login(const std::string& password);
  bool HasAPIToken() const;
  void LoadGame(const DiscIO::Volume* volume);
  bool IsGameLoaded() const;
  void SetBackgroundExecutionAllowed(bool allowed);

  static std::string CalculateHash(const std::string& file_path);

  void FetchPlayerBadge();
  void FetchGameBadges();

  void DoFrame();

  bool CanPause();
  void DoIdle();

  std::recursive_mutex& GetLock();
  bool IsHardcoreModeActive() const;

  void FilterApprovedPatches(std::vector<PatchEngine::Patch>& patches, const std::string& game_id,
                             u16 revision) const;
  void FilterApprovedGeckoCodes(std::vector<Gecko::GeckoCode>& codes, const std::string& game_id,
                                u16 revision) const;
  void FilterApprovedARCodes(std::vector<ActionReplay::ARCode>& codes, const std::string& game_id,
                             u16 revision) const;
  bool ShouldGeckoCodeBeActivated(const Gecko::GeckoCode& code, const std::string& game_id,
                                  u16 revision) const;
  bool ShouldARCodeBeActivated(const ActionReplay::ARCode& code, const std::string& game_id,
                               u16 revision) const;
  bool IsApprovedGeckoCode(const Gecko::GeckoCode& code, const std::string& game_id,
                           u16 revision) const;
  bool IsApprovedARCode(const ActionReplay::ARCode& code, const std::string& game_id,
                        u16 revision) const;

  void SetSpectatorMode();
  std::string_view GetPlayerDisplayName() const;
  u32 GetPlayerScore() const;
  const Badge& GetPlayerBadge() const;
  std::string_view GetGameDisplayName() const;
  rc_client_t* GetClient();
  const Badge& GetGameBadge() const;
  const Badge& GetAchievementBadge(AchievementId id, bool locked) const;
  const LeaderboardStatus* GetLeaderboardInfo(AchievementId leaderboard_id);
  RichPresence GetRichPresence() const;
  bool AreChallengesUpdated() const;
  void ResetChallengesUpdated();
  const std::unordered_set<AchievementId>& GetActiveChallenges() const;
  std::vector<std::string> GetActiveLeaderboards() const;

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  Common::HookableEvent<> dev_menu_update_event;
  const rc_client_raintegration_menu_t* GetDevelopmentMenu();
  u32 ActivateDevMenuItem(u32 menu_item_id);
  bool CheckForModifications() { return rc_client_raintegration_has_modifications(m_client); }
#endif  // RC_CLIENT_SUPPORTS_RAINTEGRATION

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

  static picojson::value LoadApprovedList();

  static void* FilereaderOpen(const char* path_utf8);
  static void FilereaderSeek(void* file_handle, int64_t offset, int origin);
  static int64_t FilereaderTell(void* file_handle);
  static size_t FilereaderRead(void* file_handle, void* buffer, size_t requested_bytes);
  static void FilereaderClose(void* file_handle);

  static u32 FindConsoleID(const DiscIO::Platform& platform);

  void LoadDefaultBadges();
  static void LoginCallback(int result, const char* error_message, rc_client_t* client,
                            void* userdata);

  void FetchBoardInfo(AchievementId leaderboard_id);

  std::unique_ptr<DiscIO::Volume>& GetLoadingVolume() { return m_loading_volume; }

  static void LoadGameCallback(int result, const char* error_message, rc_client_t* client,
                               void* userdata);
  static void ChangeMediaCallback(int result, const char* error_message, rc_client_t* client,
                                  void* userdata);
  void DisplayWelcomeMessage();

  void SetHardcoreMode();

  template <typename T>
  void FilterApprovedIni(std::vector<T>& codes, const std::string& game_id, u16 revision) const;
  template <typename T>
  bool ShouldCodeBeActivated(const T& code, const std::string& game_id, u16 revision) const;
  template <typename T>
  bool IsApprovedCode(const T& code, const std::string& game_id, u16 revision) const;
  Common::SHA1::Digest GetCodeHash(const PatchEngine::Patch& patch) const;
  Common::SHA1::Digest GetCodeHash(const Gecko::GeckoCode& code) const;
  Common::SHA1::Digest GetCodeHash(const ActionReplay::ARCode& code) const;

  static void LeaderboardEntriesCallback(int result, const char* error_message,
                                         rc_client_leaderboard_entry_list_t* list,
                                         rc_client_t* client, void* userdata);

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
  static void HandleResetEvent(const rc_client_event_t* client_event);
  static void HandleServerErrorEvent(const rc_client_event_t* client_event);

  static void Request(const rc_api_request_t* request, rc_client_server_callback_t callback,
                      void* callback_data, rc_client_t* client);
  static u32 MemoryPeeker(u32 address, u8* buffer, u32 num_bytes, rc_client_t* client);
  void FetchBadge(Badge* badge, u32 badge_type, const BadgeNameFunction function,
                  const UpdatedItems callback_data);
  static void EventHandler(const rc_client_event_t* event, rc_client_t* client);

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  static void LoadIntegrationCallback(int result, const char* error_message, rc_client_t* client,
                                      void* userdata);
  static void RAIntegrationEventHandler(const rc_client_raintegration_event_t* event,
                                        rc_client_t* client);
  static void MemoryPoker(u32 address, u8* buffer, u32 num_bytes, rc_client_t* client);
  static void GameTitleEstimateHandler(char* buffer, u32 buffer_size, rc_client_t* client);
#endif  // RC_CLIENT_SUPPORTS_RAINTEGRATION

  rc_client_t* m_client{};
  std::atomic<Core::System*> m_system{};
  std::unique_ptr<DiscIO::Volume> m_loading_volume;
  Config::ConfigChangedCallbackID m_config_changed_callback_id;
  Badge m_default_player_badge;
  Badge m_default_game_badge;
  Badge m_default_unlocked_badge;
  Badge m_default_locked_badge;
  std::atomic_bool m_background_execution_allowed = true;
  Badge m_player_badge;
  Badge m_game_badge;
  bool m_display_welcome_message = false;
  std::unordered_map<AchievementId, Badge> m_unlocked_badges;
  std::unordered_map<AchievementId, Badge> m_locked_badges;
  RichPresence m_rich_presence;
  std::chrono::steady_clock::time_point m_last_rp_time = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point m_last_progress_message = std::chrono::steady_clock::now();

  Common::Lazy<picojson::value> m_ini_root{LoadApprovedList};

  std::unordered_map<AchievementId, LeaderboardStatus> m_leaderboard_map;
  bool m_challenges_updated = false;
  std::unordered_set<AchievementId> m_active_challenges;
  std::vector<rc_client_leaderboard_tracker_t> m_active_leaderboards;

  bool m_dll_found = false;
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  std::string m_title_estimate;
#endif  // RC_CLIENT_SUPPORTS_RAINTEGRATION

  Common::AsyncWorkThread m_queue;
  Common::AsyncWorkThread m_image_queue;
  mutable std::recursive_mutex m_lock;
  std::recursive_mutex m_filereader_lock;
};  // class AchievementManager

#else  // USE_RETRO_ACHIEVEMENTS

#include <string>

#include "Common/CommonTypes.h"
#include "Core/ActionReplay.h"
#include "Core/GeckoCode.h"

namespace DiscIO
{
class Volume;
}

class AchievementManager
{
public:
  static AchievementManager& GetInstance()
  {
    static AchievementManager s_instance;
    return s_instance;
  }

  constexpr bool IsHardcoreModeActive() { return false; }

  constexpr bool ShouldGeckoCodeBeActivated(const Gecko::GeckoCode& code,
                                            const std::string& game_id, u16 revision)
  {
    return code.enabled;
  }

  constexpr bool ShouldARCodeBeActivated(const ActionReplay::ARCode& code,
                                         const std::string& game_id, u16 revision)
  {
    return code.enabled;
  }

  constexpr void LoadGame(const DiscIO::Volume*) {}

  constexpr void SetBackgroundExecutionAllowed(bool allowed) {}

  constexpr void DoFrame() {}

  constexpr void CloseGame() {}
};

#endif  // USE_RETRO_ACHIEVEMENTS
