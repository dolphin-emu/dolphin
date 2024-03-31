// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS

#include "Core/AchievementManager.h"

#include <cctype>
#include <memory>

#include <fmt/format.h>

#include <rcheevos/include/rc_api_info.h>
#include <rcheevos/include/rc_hash.h>

#include "Common/Image.h"
#include "Common/Logging/Log.h"
#include "Common/WorkQueueThread.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Core.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"
#include "DiscIO/Blob.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoEvents.h"

static std::unique_ptr<OSD::Icon> DecodeBadgeToOSDIcon(const AchievementManager::Badge& badge);

AchievementManager& AchievementManager::GetInstance()
{
  static AchievementManager s_instance;
  return s_instance;
}

void AchievementManager::Init()
{
  if (!m_client && Config::Get(Config::RA_ENABLED))
  {
    m_client = rc_client_create(MemoryPeeker, RequestV2);
    std::string host_url = Config::Get(Config::RA_HOST_URL);
    if (!host_url.empty())
      rc_client_set_host(m_client, host_url.c_str());
    rc_client_set_event_handler(m_client, EventHandlerV2);
    rc_client_enable_logging(m_client, RC_CLIENT_LOG_LEVEL_VERBOSE,
                             [](const char* message, const rc_client_t* client) {
                               INFO_LOG_FMT(ACHIEVEMENTS, "{}", message);
                             });
    rc_client_set_hardcore_enabled(m_client, 0);
    rc_client_set_unofficial_enabled(m_client, 1);
    m_queue.Reset("AchievementManagerQueue", [](const std::function<void()>& func) { func(); });
    m_image_queue.Reset("AchievementManagerImageQueue",
                        [](const std::function<void()>& func) { func(); });
    if (HasAPIToken())
      Login("");
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager Initialized");
  }
}

void AchievementManager::SetUpdateCallback(UpdateCallback callback)
{
  m_update_callback = std::move(callback);

  if (!m_update_callback)
    m_update_callback = [] {};

  m_update_callback();
}

void AchievementManager::Login(const std::string& password)
{
  if (!m_client)
  {
    ERROR_LOG_FMT(
        ACHIEVEMENTS,
        "Attempted login to RetroAchievements server without achievement client initialized.");
    return;
  }
  if (password.empty())
  {
    rc_client_begin_login_with_token(m_client, Config::Get(Config::RA_USERNAME).c_str(),
                                     Config::Get(Config::RA_API_TOKEN).c_str(), LoginCallback,
                                     nullptr);
  }
  else
  {
    rc_client_begin_login_with_password(m_client, Config::Get(Config::RA_USERNAME).c_str(),
                                        password.c_str(), LoginCallback, nullptr);
  }
}

bool AchievementManager::HasAPIToken() const
{
  return !Config::Get(Config::RA_API_TOKEN).empty();
}

void AchievementManager::LoadGame(const std::string& file_path, const DiscIO::Volume* volume)
{
  if (!Config::Get(Config::RA_ENABLED) || !HasAPIToken())
  {
    return;
  }
  if (file_path.empty() && volume == nullptr)
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Called Load Game without a game.");
    return;
  }
  if (!m_client)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS,
                  "Attempted to load game achievements without achievement client initialized.");
    return;
  }
  if (m_disabled)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager is disabled until core is rebooted.");
    OSD::AddMessage("Achievements are disabled until you restart emulation.",
                    OSD::Duration::VERY_LONG, OSD::Color::RED);
    return;
  }
  if (volume)
  {
    std::lock_guard lg{m_lock};
    if (!m_loading_volume)
    {
      m_loading_volume = DiscIO::CreateVolume(volume->GetBlobReader().CopyReader());
    }
  }
  std::lock_guard lg{m_filereader_lock};
  rc_hash_filereader volume_reader{
      .open = (volume) ? &AchievementManager::FilereaderOpenByVolume :
                         &AchievementManager::FilereaderOpenByFilepath,
      .seek = &AchievementManager::FilereaderSeek,
      .tell = &AchievementManager::FilereaderTell,
      .read = &AchievementManager::FilereaderRead,
      .close = &AchievementManager::FilereaderClose,
  };
  rc_hash_init_custom_filereader(&volume_reader);
  rc_client_begin_identify_and_load_game(m_client, RC_CONSOLE_GAMECUBE, file_path.c_str(), NULL, 0,
                                         LoadGameCallback, NULL);
}

bool AchievementManager::IsGameLoaded() const
{
  auto* game_info = rc_client_get_game_info(m_client);
  return game_info && game_info->id != 0;
}

void AchievementManager::FetchPlayerBadge()
{
  FetchBadge(&m_player_badge, RC_IMAGE_TYPE_USER, [](const AchievementManager& manager) {
    auto* user_info = rc_client_get_user_info(manager.m_client);
    if (!user_info)
      return std::string("");
    return std::string(user_info->display_name);
  });
}

void AchievementManager::FetchGameBadges()
{
  FetchBadge(&m_game_badge, RC_IMAGE_TYPE_GAME, [](const AchievementManager& manager) {
    auto* game_info = rc_client_get_game_info(manager.m_client);
    if (!game_info)
      return std::string("");
    return std::string(game_info->badge_name);
  });

  if (!rc_client_has_achievements(m_client))
    return;

  rc_client_achievement_list_t* achievement_list;
  {
    std::lock_guard lg{m_lock};
    achievement_list = rc_client_create_achievement_list(
        m_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  }
  for (u32 bx = 0; bx < achievement_list->num_buckets; bx++)
  {
    auto& bucket = achievement_list->buckets[bx];
    for (u32 achievement = 0; achievement < bucket.num_achievements; achievement++)
    {
      u32 achievement_id = bucket.achievements[achievement]->id;

      FetchBadge(
          &m_unlocked_badges[achievement_id], RC_IMAGE_TYPE_ACHIEVEMENT,
          [achievement_id](const AchievementManager& manager) {
            if (!rc_client_get_achievement_info(manager.m_client, achievement_id))
              return std::string("");
            return std::string(
                rc_client_get_achievement_info(manager.m_client, achievement_id)->badge_name);
          });
      FetchBadge(
          &m_locked_badges[achievement_id], RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED,
          [achievement_id](const AchievementManager& manager) {
            if (!rc_client_get_achievement_info(manager.m_client, achievement_id))
              return std::string("");
            return std::string(
                rc_client_get_achievement_info(manager.m_client, achievement_id)->badge_name);
          });
    }
  }
  rc_client_destroy_achievement_list(achievement_list);
}

void AchievementManager::DoFrame()
{
  if (!IsGameLoaded() || !Core::IsCPUThread())
    return;
  if (m_framecount == 0x200)
  {
    DisplayWelcomeMessage();
  }
  if (m_framecount <= 0x200)
  {
    m_framecount++;
  }
  {
    std::lock_guard lg{m_lock};
    rc_client_do_frame(m_client);
  }
  if (!m_system)
    return;
  time_t current_time = std::time(nullptr);
  if (difftime(current_time, m_last_ping_time) > 120)
  {
    GenerateRichPresence(Core::CPUThreadGuard{*m_system});
    m_queue.EmplaceItem([this] { PingRichPresence(m_rich_presence); });
    m_last_ping_time = current_time;
    m_update_callback();
  }
}

void AchievementManager::AchievementEventHandler(const rc_runtime_event_t* runtime_event)
{
  switch (runtime_event->type)
  {
  case RC_RUNTIME_EVENT_ACHIEVEMENT_PROGRESS_UPDATED:
    HandleAchievementProgressUpdatedEvent(runtime_event);
    break;
  case RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED:
    HandleAchievementPrimedEvent(runtime_event);
    break;
  case RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED:
    HandleAchievementUnprimedEvent(runtime_event);
    break;
  }
}

std::recursive_mutex& AchievementManager::GetLock()
{
  return m_lock;
}

bool AchievementManager::IsHardcoreModeActive() const
{
  std::lock_guard lg{m_lock};
  if (!Config::Get(Config::RA_HARDCORE_ENABLED))
    return false;
  if (!Core::IsRunning())
    return true;
  if (!IsGameLoaded())
    return false;
  return (m_runtime.trigger_count + m_runtime.lboard_count > 0);
}

std::string_view AchievementManager::GetPlayerDisplayName() const
{
  if (!HasAPIToken())
    return "";
  auto* user = rc_client_get_user_info(m_client);
  if (!user)
    return "";
  return std::string_view(user->display_name);
}

u32 AchievementManager::GetPlayerScore() const
{
  if (!HasAPIToken())
    return 0;
  auto* user = rc_client_get_user_info(m_client);
  if (!user)
    return 0;
  return user->score;
}

const AchievementManager::BadgeStatus& AchievementManager::GetPlayerBadge() const
{
  return m_player_badge;
}

std::string_view AchievementManager::GetGameDisplayName() const
{
  return IsGameLoaded() ? std::string_view(rc_client_get_game_info(m_client)->title) : "";
}

AchievementManager::PointSpread AchievementManager::TallyScore() const
{
  PointSpread spread{};
  if (!IsGameLoaded())
    return spread;
  bool hardcore_mode_enabled = Config::Get(Config::RA_HARDCORE_ENABLED);
  for (const auto& entry : m_unlock_map)
  {
    if (entry.second.category != RC_ACHIEVEMENT_CATEGORY_CORE)
      continue;
    u32 points = entry.second.points;
    spread.total_count++;
    spread.total_points += points;
    if (entry.second.remote_unlock_status == UnlockStatus::UnlockType::HARDCORE ||
        (hardcore_mode_enabled && entry.second.session_unlock_count > 0))
    {
      spread.hard_unlocks++;
      spread.hard_points += points;
    }
    else if (entry.second.remote_unlock_status == UnlockStatus::UnlockType::SOFTCORE ||
             entry.second.session_unlock_count > 0)
    {
      spread.soft_unlocks++;
      spread.soft_points += points;
    }
  }
  return spread;
}

rc_api_fetch_game_data_response_t* AchievementManager::GetGameData()
{
  return &m_game_data;
}

const AchievementManager::BadgeStatus& AchievementManager::GetGameBadge() const
{
  return m_game_badge;
}

const AchievementManager::UnlockStatus&
AchievementManager::GetUnlockStatus(AchievementId achievement_id) const
{
  return m_unlock_map.at(achievement_id);
}

AchievementManager::ResponseType
AchievementManager::GetAchievementProgress(AchievementId achievement_id, u32* value, u32* target)
{
  if (!IsGameLoaded())
  {
    ERROR_LOG_FMT(
        ACHIEVEMENTS,
        "Attempted to request measured data for achievement ID {} when no game is running.",
        achievement_id);
    return ResponseType::INVALID_REQUEST;
  }
  int result = rc_runtime_get_achievement_measured(&m_runtime, achievement_id, value, target);
  if (result == 0)
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Failed to get measured data for achievement ID {}.",
                 achievement_id);
    return ResponseType::MALFORMED_OBJECT;
  }
  return ResponseType::SUCCESS;
}

const std::unordered_map<AchievementManager::AchievementId, AchievementManager::LeaderboardStatus>&
AchievementManager::GetLeaderboardsInfo() const
{
  return m_leaderboard_map;
}

AchievementManager::RichPresence AchievementManager::GetRichPresence() const
{
  std::lock_guard lg{m_lock};
  return m_rich_presence;
}

void AchievementManager::SetDisabled(bool disable)
{
  bool previously_disabled;
  {
    std::lock_guard lg{m_lock};
    previously_disabled = m_disabled;
    m_disabled = disable;
    if (disable && m_is_game_loaded)
      CloseGame();
  }

  if (!previously_disabled && disable && Config::Get(Config::RA_ENABLED))
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager has been disabled.");
    OSD::AddMessage("Please close all games to re-enable achievements.", OSD::Duration::VERY_LONG,
                    OSD::Color::RED);
    m_update_callback();
  }

  if (previously_disabled && !disable)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager has been re-enabled.");
    m_update_callback();
  }
};

const AchievementManager::NamedIconMap& AchievementManager::GetChallengeIcons() const
{
  return m_active_challenges;
}

void AchievementManager::CloseGame()
{
  {
    std::lock_guard lg{m_lock};
    if (rc_client_get_game_info(m_client))
    {
      m_active_challenges.clear();
      m_game_badge.name.clear();
      m_unlocked_badges.clear();
      m_locked_badges.clear();
      m_unlock_map.clear();
      m_leaderboard_map.clear();
      rc_api_destroy_fetch_game_data_response(&m_game_data);
      m_game_data = {};
      m_queue.Cancel();
      m_image_queue.Cancel();
      rc_client_unload_game(m_client);
      m_system = nullptr;
    }
  }

  m_update_callback();
  INFO_LOG_FMT(ACHIEVEMENTS, "Game closed.");
}

void AchievementManager::Logout()
{
  {
    std::lock_guard lg{m_lock};
    CloseGame();
    SetDisabled(false);
    m_player_badge.name.clear();
    Config::SetBaseOrCurrent(Config::RA_API_TOKEN, "");
  }

  m_update_callback();
  INFO_LOG_FMT(ACHIEVEMENTS, "Logged out from server.");
}

void AchievementManager::Shutdown()
{
  if (m_client)
  {
    CloseGame();
    SetDisabled(false);
    m_queue.Shutdown();
    // DON'T log out - keep those credentials for next run.
    rc_client_destroy(m_client);
    m_client = nullptr;
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager shut down.");
  }
}

void* AchievementManager::FilereaderOpenByFilepath(const char* path_utf8)
{
  auto state = std::make_unique<FilereaderState>();
  state->volume = DiscIO::CreateVolume(path_utf8);
  if (!state->volume)
    return nullptr;
  return state.release();
}

void* AchievementManager::FilereaderOpenByVolume(const char* path_utf8)
{
  auto state = std::make_unique<FilereaderState>();
  {
    auto& instance = GetInstance();
    std::lock_guard lg{instance.GetLock()};
    state->volume = std::move(instance.GetLoadingVolume());
  }
  if (!state->volume)
    return nullptr;
  return state.release();
}

void AchievementManager::FilereaderSeek(void* file_handle, int64_t offset, int origin)
{
  switch (origin)
  {
  case SEEK_SET:
    static_cast<FilereaderState*>(file_handle)->position = offset;
    break;
  case SEEK_CUR:
    static_cast<FilereaderState*>(file_handle)->position += offset;
    break;
  case SEEK_END:
    // Unused
    break;
  }
}

int64_t AchievementManager::FilereaderTell(void* file_handle)
{
  return static_cast<FilereaderState*>(file_handle)->position;
}

size_t AchievementManager::FilereaderRead(void* file_handle, void* buffer, size_t requested_bytes)
{
  FilereaderState* filereader_state = static_cast<FilereaderState*>(file_handle);
  bool success = (filereader_state->volume->Read(filereader_state->position, requested_bytes,
                                                 static_cast<u8*>(buffer), DiscIO::PARTITION_NONE));
  if (success)
  {
    filereader_state->position += requested_bytes;
    return requested_bytes;
  }
  else
  {
    return 0;
  }
}

void AchievementManager::FilereaderClose(void* file_handle)
{
  delete static_cast<FilereaderState*>(file_handle);
}

void AchievementManager::LoginCallback(int result, const char* error_message, rc_client_t* client,
                                       void* userdata)
{
  if (result != RC_OK)
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Failed to login {} to RetroAchievements server.",
                 Config::Get(Config::RA_USERNAME));
    return;
  }

  const rc_client_user_t* user;
  {
    std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
    user = rc_client_get_user_info(client);
  }
  if (!user)
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Failed to retrieve user information from client.");
    return;
  }

  std::string config_username = Config::Get(Config::RA_USERNAME);
  if (config_username != user->username)
  {
    if (Common::CaseInsensitiveEquals(config_username, user->username))
    {
      INFO_LOG_FMT(ACHIEVEMENTS,
                   "Case mismatch between site {} and local {}; updating local config.",
                   user->username, Config::Get(Config::RA_USERNAME));
      Config::SetBaseOrCurrent(Config::RA_USERNAME, user->username);
    }
    else
    {
      INFO_LOG_FMT(ACHIEVEMENTS, "Attempted to login prior user {}; current user is {}.",
                   user->username, Config::Get(Config::RA_USERNAME));
      rc_client_logout(client);
      return;
    }
  }
  INFO_LOG_FMT(ACHIEVEMENTS, "Successfully logged in {} to RetroAchievements server.",
               user->username);
  std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
  Config::SetBaseOrCurrent(Config::RA_API_TOKEN, user->token);
  AchievementManager::GetInstance().FetchPlayerBadge();
}

AchievementManager::ResponseType AchievementManager::FetchBoardInfo(AchievementId leaderboard_id)
{
  std::string username = Config::Get(Config::RA_USERNAME);
  LeaderboardStatus lboard{};

  {
    rc_api_fetch_leaderboard_info_response_t board_info{};
    const rc_api_fetch_leaderboard_info_request_t fetch_board_request = {
        .leaderboard_id = leaderboard_id, .count = 4, .first_entry = 1, .username = nullptr};
    const ResponseType r_type =
        Request<rc_api_fetch_leaderboard_info_request_t, rc_api_fetch_leaderboard_info_response_t>(
            fetch_board_request, &board_info, rc_api_init_fetch_leaderboard_info_request,
            rc_api_process_fetch_leaderboard_info_response);
    if (r_type != ResponseType::SUCCESS)
    {
      ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to fetch info for leaderboard ID {}.", leaderboard_id);
      rc_api_destroy_fetch_leaderboard_info_response(&board_info);
      return r_type;
    }
    lboard.name = board_info.title;
    lboard.description = board_info.description;
    lboard.entries.clear();
    for (u32 i = 0; i < board_info.num_entries; ++i)
    {
      const auto& org_entry = board_info.entries[i];
      auto dest_entry = LeaderboardEntry{
          .username = org_entry.username,
          .rank = org_entry.rank,
      };
      if (rc_runtime_format_lboard_value(dest_entry.score.data(), FORMAT_SIZE, org_entry.score,
                                         board_info.format) == 0)
      {
        ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to format leaderboard score {}.", org_entry.score);
        strncpy(dest_entry.score.data(), fmt::format("{}", org_entry.score).c_str(), FORMAT_SIZE);
      }
      lboard.entries.insert_or_assign(org_entry.index, std::move(dest_entry));
    }
    rc_api_destroy_fetch_leaderboard_info_response(&board_info);
  }

  {
    // Retrieve, if exists, the player's entry, the two entries above the player, and the two
    // entries below the player, for a total of five entries. Technically I only need one entry
    // below, but the API is ambiguous what happens if an even number and a username are provided.
    rc_api_fetch_leaderboard_info_response_t board_info{};
    const rc_api_fetch_leaderboard_info_request_t fetch_board_request = {
        .leaderboard_id = leaderboard_id,
        .count = 5,
        .first_entry = 0,
        .username = username.c_str()};
    const ResponseType r_type =
        Request<rc_api_fetch_leaderboard_info_request_t, rc_api_fetch_leaderboard_info_response_t>(
            fetch_board_request, &board_info, rc_api_init_fetch_leaderboard_info_request,
            rc_api_process_fetch_leaderboard_info_response);
    if (r_type != ResponseType::SUCCESS)
    {
      ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to fetch info for leaderboard ID {}.", leaderboard_id);
      rc_api_destroy_fetch_leaderboard_info_response(&board_info);
      return r_type;
    }
    for (u32 i = 0; i < board_info.num_entries; ++i)
    {
      const auto& org_entry = board_info.entries[i];
      auto dest_entry = LeaderboardEntry{
          .username = org_entry.username,
          .rank = org_entry.rank,
      };
      if (rc_runtime_format_lboard_value(dest_entry.score.data(), FORMAT_SIZE, org_entry.score,
                                         board_info.format) == 0)
      {
        ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to format leaderboard score {}.", org_entry.score);
        strncpy(dest_entry.score.data(), fmt::format("{}", org_entry.score).c_str(), FORMAT_SIZE);
      }
      lboard.entries.insert_or_assign(org_entry.index, std::move(dest_entry));
      if (org_entry.username == username)
        lboard.player_index = org_entry.index;
    }
    rc_api_destroy_fetch_leaderboard_info_response(&board_info);
  }

  {
    std::lock_guard lg{m_lock};
    m_leaderboard_map.insert_or_assign(leaderboard_id, std::move(lboard));
  }

  return ResponseType::SUCCESS;
}

void AchievementManager::GenerateRichPresence(const Core::CPUThreadGuard& guard)
{
  std::lock_guard lg{m_lock};
  rc_runtime_get_richpresence(
      &m_runtime, m_rich_presence.data(), RP_SIZE,
      [](unsigned address, unsigned num_bytes, void* ud) { return 0u; }, this, nullptr);
}

AchievementManager::ResponseType
AchievementManager::PingRichPresence(const RichPresence& rich_presence)
{
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_ping_request_t ping_request = {.username = username.c_str(),
                                        .api_token = api_token.c_str(),
                                        .game_id = m_game_id,
                                        .rich_presence = rich_presence.data()};
  rc_api_ping_response_t ping_response = {};
  ResponseType r_type = Request<rc_api_ping_request_t, rc_api_ping_response_t>(
      ping_request, &ping_response, rc_api_init_ping_request, rc_api_process_ping_response);
  rc_api_destroy_ping_response(&ping_response);
  return r_type;
}

void AchievementManager::LoadGameCallback(int result, const char* error_message,
                                          rc_client_t* client, void* userdata)
{
  if (result != RC_OK)
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Failed to load data for current game.");
    return;
  }

  auto* game = rc_client_get_game_info(client);
  if (!game)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to retrieve game information from client.");
    return;
  }
  INFO_LOG_FMT(ACHIEVEMENTS, "Loaded data for game ID {}.", game->id);

  AchievementManager::GetInstance().FetchGameBadges();
  AchievementManager::GetInstance().m_system = &Core::System::GetInstance();
}

void AchievementManager::DisplayWelcomeMessage()
{
  std::lock_guard lg{m_lock};
  const u32 color =
      rc_client_get_hardcore_enabled(m_client) ? OSD::Color::YELLOW : OSD::Color::CYAN;
  if (Config::Get(Config::RA_BADGES_ENABLED))
  {
    OSD::AddMessage("", OSD::Duration::VERY_LONG, OSD::Color::GREEN,
                    DecodeBadgeToOSDIcon(m_game_badge.badge));
  }
  auto info = rc_client_get_game_info(m_client);
  if (!info)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Attempting to welcome player to game not running.");
    return;
  }
  OSD::AddMessage(info->title, OSD::Duration::VERY_LONG, OSD::Color::GREEN);
  rc_client_user_game_summary_t summary;
  rc_client_get_user_game_summary(m_client, &summary);
  OSD::AddMessage(fmt::format("You have {}/{} achievements worth {}/{} points",
                              summary.num_unlocked_achievements, summary.num_core_achievements,
                              summary.points_unlocked, summary.points_core),
                  OSD::Duration::VERY_LONG, color);
  if (summary.num_unsupported_achievements > 0)
  {
    OSD::AddMessage(
        fmt::format("{} achievements unsupported", summary.num_unsupported_achievements),
        OSD::Duration::VERY_LONG, OSD::Color::RED);
  }
  OSD::AddMessage(
      fmt::format("Hardcore mode is {}", rc_client_get_hardcore_enabled(m_client) ? "ON" : "OFF"),
      OSD::Duration::VERY_LONG, color);
  OSD::AddMessage(fmt::format("Leaderboard submissions are {}",
                              Config::Get(Config::RA_LEADERBOARDS_ENABLED) ? "ON" : "OFF"),
                  OSD::Duration::VERY_LONG, color);
}

void AchievementManager::HandleAchievementProgressUpdatedEvent(
    const rc_runtime_event_t* runtime_event)
{
  if (!Config::Get(Config::RA_PROGRESS_ENABLED))
    return;
  auto it = m_unlock_map.find(runtime_event->id);
  if (it == m_unlock_map.end())
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid achievement progress updated event with id {}.",
                  runtime_event->id);
    return;
  }
  AchievementId game_data_index = it->second.game_data_index;
  FormattedValue value{};
  if (rc_runtime_format_achievement_measured(&m_runtime, runtime_event->id, value.data(),
                                             FORMAT_SIZE) == 0)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to format measured data {}.", value.data());
    return;
  }
  OSD::AddMessage(
      fmt::format("{} {}", m_game_data.achievements[game_data_index].title, value.data()),
      OSD::Duration::SHORT, OSD::Color::GREEN,
      (Config::Get(Config::RA_BADGES_ENABLED)) ?
          DecodeBadgeToOSDIcon(it->second.unlocked_badge.badge) :
          nullptr);
}

void AchievementManager::HandleAchievementPrimedEvent(const rc_runtime_event_t* runtime_event)
{
  if (!Config::Get(Config::RA_BADGES_ENABLED))
    return;
  auto it = m_unlock_map.find(runtime_event->id);
  if (it == m_unlock_map.end())
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid achievement primed event with id {}.", runtime_event->id);
    return;
  }
  m_active_challenges[it->second.unlocked_badge.name] =
      DecodeBadgeToOSDIcon(it->second.unlocked_badge.badge);
}

void AchievementManager::HandleAchievementUnprimedEvent(const rc_runtime_event_t* runtime_event)
{
  if (!Config::Get(Config::RA_BADGES_ENABLED))
    return;
  auto it = m_unlock_map.find(runtime_event->id);
  if (it == m_unlock_map.end())
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid achievement unprimed event with id {}.",
                  runtime_event->id);
    return;
  }
  m_active_challenges.erase(it->second.unlocked_badge.name);
}

void AchievementManager::HandleAchievementTriggeredEvent(const rc_client_event_t* client_event)
{
  OSD::AddMessage(fmt::format("Unlocked: {} ({})", client_event->achievement->title,
                              client_event->achievement->points),
                  OSD::Duration::VERY_LONG,
                  (rc_client_get_hardcore_enabled(AchievementManager::GetInstance().m_client)) ?
                      OSD::Color::YELLOW :
                      OSD::Color::CYAN,
                  (Config::Get(Config::RA_BADGES_ENABLED)) ?
                      DecodeBadgeToOSDIcon(AchievementManager::GetInstance()
                                               .m_unlocked_badges[client_event->achievement->id]
                                               .badge) :
                      nullptr);
}

void AchievementManager::HandleLeaderboardStartedEvent(const rc_client_event_t* client_event)
{
  OSD::AddMessage(fmt::format("Attempting leaderboard: {} - {}", client_event->leaderboard->title,
                              client_event->leaderboard->description),
                  OSD::Duration::VERY_LONG, OSD::Color::GREEN);
}

void AchievementManager::HandleLeaderboardFailedEvent(const rc_client_event_t* client_event)
{
  OSD::AddMessage(fmt::format("Failed leaderboard: {}", client_event->leaderboard->title),
                  OSD::Duration::VERY_LONG, OSD::Color::RED);
}

void AchievementManager::HandleLeaderboardSubmittedEvent(const rc_client_event_t* client_event)
{
  OSD::AddMessage(fmt::format("Scored {} on leaderboard: {}",
                              client_event->leaderboard->tracker_value,
                              client_event->leaderboard->title),
                  OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
}

// Every RetroAchievements API call, with only a partial exception for fetch_image, follows
// the same design pattern (here, X is the name of the call):
//   Create a specific rc_api_X_request_t struct and populate with the necessary values
//   Call rc_api_init_X_request to convert this into a generic rc_api_request_t struct
//   Perform the HTTP request using the url and post_data in the rc_api_request_t struct
//   Call rc_api_process_X_response to convert the raw string HTTP response into a
//     rc_api_X_response_t struct
//   Use the data in the rc_api_X_response_t struct as needed
//   Call rc_api_destroy_X_response when finished with the response struct to free memory
template <typename RcRequest, typename RcResponse>
AchievementManager::ResponseType AchievementManager::Request(
    RcRequest rc_request, RcResponse* rc_response,
    const std::function<int(rc_api_request_t*, const RcRequest*)>& init_request,
    const std::function<int(RcResponse*, const char*)>& process_response)
{
  rc_api_request_t api_request;
  Common::HttpRequest http_request;
  if (init_request(&api_request, &rc_request) != RC_OK || !api_request.post_data)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid API request.");
    return ResponseType::INVALID_REQUEST;
  }
  auto http_response = http_request.Post(api_request.url, api_request.post_data);
  rc_api_destroy_request(&api_request);
  if (http_response.has_value() && http_response->size() > 0)
  {
    const std::string response_str(http_response->begin(), http_response->end());
    if (process_response(rc_response, response_str.c_str()) != RC_OK)
    {
      ERROR_LOG_FMT(ACHIEVEMENTS, "Failed to process HTTP response. \nURL: {} \nresponse: {}",
                    api_request.url, response_str);
      return ResponseType::MALFORMED_OBJECT;
    }
    if (rc_response->response.succeeded)
    {
      return ResponseType::SUCCESS;
    }
    else
    {
      Logout();
      WARN_LOG_FMT(ACHIEVEMENTS, "Invalid RetroAchievements credentials; failed login.");
      return ResponseType::INVALID_CREDENTIALS;
    }
  }
  else
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "RetroAchievements connection failed. \nURL: {}", api_request.url);
    return ResponseType::CONNECTION_FAILED;
  }
}

static std::unique_ptr<OSD::Icon> DecodeBadgeToOSDIcon(const AchievementManager::Badge& badge)
{
  if (badge.empty())
    return nullptr;

  auto icon = std::make_unique<OSD::Icon>();
  if (!Common::LoadPNG(badge, &icon->rgba_data, &icon->width, &icon->height))
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Error decoding badge.");
    return nullptr;
  }
  return icon;
}

void AchievementManager::RequestV2(const rc_api_request_t* request,
                                   rc_client_server_callback_t callback, void* callback_data,
                                   rc_client_t* client)
{
  std::string url = request->url;
  std::string post_data = request->post_data;
  AchievementManager::GetInstance().m_queue.EmplaceItem([url = std::move(url),
                                                         post_data = std::move(post_data),
                                                         callback = std::move(callback),
                                                         callback_data = std::move(callback_data)] {
    const Common::HttpRequest::Headers USER_AGENT_HEADER = {{"User-Agent", "Dolphin/Placeholder"}};

    Common::HttpRequest http_request;
    Common::HttpRequest::Response http_response;
    if (!post_data.empty())
    {
      http_response = http_request.Post(url, post_data, USER_AGENT_HEADER,
                                        Common::HttpRequest::AllowedReturnCodes::All);
    }
    else
    {
      http_response =
          http_request.Get(url, USER_AGENT_HEADER, Common::HttpRequest::AllowedReturnCodes::All);
    }

    rc_api_server_response_t server_response;
    if (http_response.has_value() && http_response->size() > 0)
    {
      server_response.body = reinterpret_cast<const char*>(http_response->data());
      server_response.body_length = http_response->size();
      server_response.http_status_code = http_request.GetLastResponseCode();
    }
    else
    {
      constexpr char error_message[] = "Failed HTTP request.";
      server_response.body = error_message;
      server_response.body_length = sizeof(error_message);
      server_response.http_status_code = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
    }

    callback(&server_response, callback_data);
  });
}

u32 AchievementManager::MemoryPeeker(u32 address, u8* buffer, u32 num_bytes, rc_client_t* client)
{
  if (buffer == nullptr)
    return 0u;
  auto& system = Core::System::GetInstance();
  Core::CPUThreadGuard threadguard(system);
  for (u32 num_read = 0; num_read < num_bytes; num_read++)
  {
    auto value = system.GetMMU().HostTryReadU8(threadguard, address + num_read,
                                               PowerPC::RequestedAddressSpace::Physical);
    if (!value.has_value())
      return num_read;
    buffer[num_read] = value.value().value;
  }
  return num_bytes;
}

void AchievementManager::FetchBadge(AchievementManager::BadgeStatus* badge, u32 badge_type,
                                    const AchievementManager::BadgeNameFunction function)
{
  if (!m_client || !HasAPIToken() || !Config::Get(Config::RA_BADGES_ENABLED))
  {
    m_update_callback();
    return;
  }

  m_image_queue.EmplaceItem([this, badge, badge_type, function = std::move(function)] {
    std::string name_to_fetch;
    {
      std::lock_guard lg{m_lock};
      name_to_fetch = function(*this);
      if (name_to_fetch.empty())
        return;
    }
    rc_api_fetch_image_request_t icon_request = {.image_name = name_to_fetch.c_str(),
                                                 .image_type = badge_type};
    Badge fetched_badge;
    rc_api_request_t api_request;
    Common::HttpRequest http_request;
    if (rc_api_init_fetch_image_request(&api_request, &icon_request) != RC_OK)
    {
      ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid request for image {}.", name_to_fetch);
      return;
    }
    auto http_response = http_request.Get(api_request.url);
    if (http_response.has_value() && http_response->size() <= 0)
    {
      WARN_LOG_FMT(ACHIEVEMENTS, "RetroAchievements connection failed on image request.\n URL: {}",
                   api_request.url);
      rc_api_destroy_request(&api_request);
      m_update_callback();
      return;
    }

    rc_api_destroy_request(&api_request);
    fetched_badge = std::move(*http_response);

    INFO_LOG_FMT(ACHIEVEMENTS, "Successfully downloaded badge id {}.", name_to_fetch);
    std::lock_guard lg{m_lock};
    if (function(*this).empty() || name_to_fetch != function(*this))
    {
      INFO_LOG_FMT(ACHIEVEMENTS, "Requested outdated badge id {}.", name_to_fetch);
      return;
    }
    badge->badge = std::move(fetched_badge);
    badge->name = std::move(name_to_fetch);

    m_update_callback();
  });
}

void AchievementManager::EventHandlerV2(const rc_client_event_t* event, rc_client_t* client)
{
  switch (event->type)
  {
  case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
    HandleAchievementTriggeredEvent(event);
    break;
  case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
    HandleLeaderboardStartedEvent(event);
    break;
  case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
    HandleLeaderboardFailedEvent(event);
    break;
  case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
    HandleLeaderboardSubmittedEvent(event);
    break;
  default:
    INFO_LOG_FMT(ACHIEVEMENTS, "Event triggered of unhandled type {}", event->type);
    break;
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
