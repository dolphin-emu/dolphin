// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS

#include "Core/AchievementManager.h"

#include <fmt/format.h>

#include <rcheevos/include/rc_hash.h>

#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/WorkQueueThread.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Core.h"
#include "Core/PowerPC/MMU.h"
#include "Core/System.h"
#include "DiscIO/Volume.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoEvents.h"

static constexpr bool hardcore_mode_enabled = false;

AchievementManager* AchievementManager::GetInstance()
{
  static AchievementManager s_instance;
  return &s_instance;
}

void AchievementManager::Init()
{
  if (!m_is_runtime_initialized && Config::Get(Config::RA_ENABLED))
  {
    rc_runtime_init(&m_runtime);
    m_is_runtime_initialized = true;
    m_queue.Reset("AchievementManagerQueue", [](const std::function<void()>& func) { func(); });
    LoginAsync("", [](ResponseType r_type) {});
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager Initialized");
  }
}

void AchievementManager::SetUpdateCallback(UpdateCallback callback)
{
  m_update_callback = std::move(callback);
  m_update_callback();
}

AchievementManager::ResponseType AchievementManager::Login(const std::string& password)
{
  if (!m_is_runtime_initialized)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Attempted login (sync) to RetroAchievements server without "
                                "Achievement Manager initialized.");
    return AchievementManager::ResponseType::MANAGER_NOT_INITIALIZED;
  }
  AchievementManager::ResponseType r_type = VerifyCredentials(password);
  if (m_update_callback)
    m_update_callback();
  return r_type;
}

void AchievementManager::LoginAsync(const std::string& password, const ResponseCallback& callback)
{
  if (!m_is_runtime_initialized)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Attempted login (async) to RetroAchievements server without "
                                "Achievement Manager initialized.");
    callback(AchievementManager::ResponseType::MANAGER_NOT_INITIALIZED);
    return;
  }
  m_queue.EmplaceItem([this, password, callback] {
    callback(VerifyCredentials(password));
    if (m_update_callback)
      m_update_callback();
  });
}

bool AchievementManager::IsLoggedIn() const
{
  return !Config::Get(Config::RA_API_TOKEN).empty();
}

void AchievementManager::LoadGameByFilenameAsync(const std::string& iso_path,
                                                 const ResponseCallback& callback)
{
  if (!m_is_runtime_initialized)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS,
                  "Attempted to load game achievements without Achievement Manager initialized.");
    callback(AchievementManager::ResponseType::MANAGER_NOT_INITIALIZED);
    return;
  }
  m_system = &Core::System::GetInstance();
  struct FilereaderState
  {
    int64_t position = 0;
    std::unique_ptr<DiscIO::Volume> volume;
  };
  rc_hash_filereader volume_reader{
      .open = [](const char* path_utf8) -> void* {
        auto state = std::make_unique<FilereaderState>();
        state->volume = DiscIO::CreateVolume(path_utf8);
        if (!state->volume)
          return nullptr;
        return state.release();
      },
      .seek =
          [](void* file_handle, int64_t offset, int origin) {
            switch (origin)
            {
            case SEEK_SET:
              reinterpret_cast<FilereaderState*>(file_handle)->position = offset;
              break;
            case SEEK_CUR:
              reinterpret_cast<FilereaderState*>(file_handle)->position += offset;
              break;
            case SEEK_END:
              // Unused
              break;
            }
          },
      .tell =
          [](void* file_handle) {
            return reinterpret_cast<FilereaderState*>(file_handle)->position;
          },
      .read =
          [](void* file_handle, void* buffer, size_t requested_bytes) {
            FilereaderState* filereader_state = reinterpret_cast<FilereaderState*>(file_handle);
            bool success = (filereader_state->volume->Read(
                filereader_state->position, requested_bytes, reinterpret_cast<u8*>(buffer),
                DiscIO::PARTITION_NONE));
            if (success)
            {
              filereader_state->position += requested_bytes;
              return requested_bytes;
            }
            else
            {
              return static_cast<size_t>(0);
            }
          },
      .close = [](void* file_handle) { delete reinterpret_cast<FilereaderState*>(file_handle); }};
  rc_hash_init_custom_filereader(&volume_reader);
  if (!rc_hash_generate_from_file(m_game_hash.data(), RC_CONSOLE_GAMECUBE, iso_path.c_str()))
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Unable to generate achievement hash from game file.");
    return;
  }
  m_queue.EmplaceItem([this, callback] {
    const auto resolve_hash_response = ResolveHash(this->m_game_hash);
    if (resolve_hash_response != ResponseType::SUCCESS || m_game_id == 0)
    {
      callback(resolve_hash_response);
      INFO_LOG_FMT(ACHIEVEMENTS, "No RetroAchievements data found for this game.");
      OSD::AddMessage("No RetroAchievements data found for this game.", OSD::Duration::VERY_LONG,
                      OSD::Color::RED);
      return;
    }

    const auto start_session_response = StartRASession();
    if (start_session_response != ResponseType::SUCCESS)
    {
      callback(start_session_response);
      WARN_LOG_FMT(ACHIEVEMENTS, "Failed to connect to RetroAchievements server.");
      OSD::AddMessage("Failed to connect to RetroAchievements server.", OSD::Duration::VERY_LONG,
                      OSD::Color::RED);
      return;
    }

    const auto fetch_game_data_response = FetchGameData();
    if (fetch_game_data_response != ResponseType::SUCCESS)
    {
      ERROR_LOG_FMT(ACHIEVEMENTS, "Unable to retrieve data from RetroAchievements server.");
      OSD::AddMessage("Unable to retrieve data from RetroAchievements server.",
                      OSD::Duration::VERY_LONG, OSD::Color::RED);
      return;
    }
    INFO_LOG_FMT(ACHIEVEMENTS, "Loading achievements for {}.", m_game_data.title);

    // Claim the lock, then queue the fetch unlock data calls, then initialize the unlock map in
    // ActivateDeactiveAchievements. This allows the calls to process while initializing the
    // unlock map but then forces them to wait until it's initialized before making modifications to
    // it.
    {
      std::lock_guard lg{m_lock};
      m_is_game_loaded = true;
      LoadUnlockData([](ResponseType r_type) {});
      ActivateDeactivateAchievements();
      PointSpread spread = TallyScore();
      if (hardcore_mode_enabled)
      {
        OSD::AddMessage(fmt::format("You have {}/{} achievements worth {}/{} points",
                                    spread.hard_unlocks, spread.total_count, spread.hard_points,
                                    spread.total_points),
                        OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
        OSD::AddMessage("Hardcore mode is ON", OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
      }
      else
      {
        OSD::AddMessage(fmt::format("You have {}/{} achievements worth {}/{} points",
                                    spread.hard_unlocks + spread.soft_unlocks, spread.total_count,
                                    spread.hard_points + spread.soft_points, spread.total_points),
                        OSD::Duration::VERY_LONG, OSD::Color::CYAN);
        OSD::AddMessage("Hardcore mode is OFF", OSD::Duration::VERY_LONG, OSD::Color::CYAN);
      }
    }
    ActivateDeactivateLeaderboards();
    ActivateDeactivateRichPresence();
    // Reset this to zero so that RP immediately triggers on the first frame
    m_last_ping_time = 0;
    INFO_LOG_FMT(ACHIEVEMENTS, "RetroAchievements successfully loaded for {}.", m_game_data.title);

    if (m_update_callback)
      m_update_callback();
    callback(fetch_game_data_response);
  });
}

bool AchievementManager::IsGameLoaded() const
{
  return m_is_game_loaded;
}

void AchievementManager::LoadUnlockData(const ResponseCallback& callback)
{
  m_queue.EmplaceItem([this, callback] {
    const auto hardcore_unlock_response = FetchUnlockData(true);
    if (hardcore_unlock_response != ResponseType::SUCCESS)
    {
      ERROR_LOG_FMT(ACHIEVEMENTS,
                    "Failed to fetch hardcore unlock data; skipping softcore unlock.");
      callback(hardcore_unlock_response);
      return;
    }

    callback(FetchUnlockData(false));
    if (m_update_callback)
      m_update_callback();
  });
}

void AchievementManager::ActivateDeactivateAchievements()
{
  bool enabled = Config::Get(Config::RA_ACHIEVEMENTS_ENABLED);
  bool unofficial = Config::Get(Config::RA_UNOFFICIAL_ENABLED);
  bool encore = Config::Get(Config::RA_ENCORE_ENABLED);
  for (u32 ix = 0; ix < m_game_data.num_achievements; ix++)
  {
    u32 points = (m_game_data.achievements[ix].category == RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL) ?
                     0 :
                     m_game_data.achievements[ix].points;
    auto iter = m_unlock_map.insert(
        {m_game_data.achievements[ix].id, UnlockStatus{.game_data_index = ix, .points = points}});
    ActivateDeactivateAchievement(iter.first->first, enabled, unofficial, encore);
  }
  INFO_LOG_FMT(ACHIEVEMENTS, "Achievements (de)activated.");
}

void AchievementManager::ActivateDeactivateLeaderboards()
{
  bool leaderboards_enabled = Config::Get(Config::RA_LEADERBOARDS_ENABLED);
  for (u32 ix = 0; ix < m_game_data.num_leaderboards; ix++)
  {
    auto leaderboard = m_game_data.leaderboards[ix];
    if (m_is_game_loaded && leaderboards_enabled && hardcore_mode_enabled)
    {
      rc_runtime_activate_lboard(&m_runtime, leaderboard.id, leaderboard.definition, nullptr, 0);
    }
    else
    {
      rc_runtime_deactivate_lboard(&m_runtime, m_game_data.leaderboards[ix].id);
    }
  }
  INFO_LOG_FMT(ACHIEVEMENTS, "Leaderboards (de)activated.");
}

void AchievementManager::ActivateDeactivateRichPresence()
{
  rc_runtime_activate_richpresence(
      &m_runtime,
      (m_is_game_loaded && Config::Get(Config::RA_RICH_PRESENCE_ENABLED)) ?
          m_game_data.rich_presence_script :
          "",
      nullptr, 0);
  INFO_LOG_FMT(ACHIEVEMENTS, "Rich presence (de)activated.");
}

void AchievementManager::DoFrame()
{
  if (!m_is_game_loaded)
    return;
  Core::RunAsCPUThread([&] {
    rc_runtime_do_frame(
        &m_runtime,
        [](const rc_runtime_event_t* runtime_event) {
          AchievementManager::GetInstance()->AchievementEventHandler(runtime_event);
        },
        [](unsigned address, unsigned num_bytes, void* ud) {
          return static_cast<AchievementManager*>(ud)->MemoryPeeker(address, num_bytes, ud);
        },
        this, nullptr);
  });
  if (!m_system)
    return;
  time_t current_time = std::time(nullptr);
  if (difftime(current_time, m_last_ping_time) > 120)
  {
    GenerateRichPresence();
    m_queue.EmplaceItem([this] { PingRichPresence(m_rich_presence); });
    m_last_ping_time = current_time;
    if (m_update_callback)
      m_update_callback();
  }
}

u32 AchievementManager::MemoryPeeker(u32 address, u32 num_bytes, void* ud)
{
  if (!m_system)
    return 0u;
  Core::CPUThreadGuard threadguard(*m_system);
  switch (num_bytes)
  {
  case 1:
    return m_system->GetMMU()
        .HostTryReadU8(threadguard, address, PowerPC::RequestedAddressSpace::Physical)
        .value_or(PowerPC::ReadResult<u8>(false, 0u))
        .value;
  case 2:
    return m_system->GetMMU()
        .HostTryReadU16(threadguard, address, PowerPC::RequestedAddressSpace::Physical)
        .value_or(PowerPC::ReadResult<u16>(false, 0u))
        .value;
  case 4:
    return m_system->GetMMU()
        .HostTryReadU32(threadguard, address, PowerPC::RequestedAddressSpace::Physical)
        .value_or(PowerPC::ReadResult<u32>(false, 0u))
        .value;
  default:
    ASSERT(false);
    return 0u;
  }
}

void AchievementManager::AchievementEventHandler(const rc_runtime_event_t* runtime_event)
{
  {
    std::lock_guard lg{m_lock};
    switch (runtime_event->type)
    {
    case RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED:
      HandleAchievementTriggeredEvent(runtime_event);
      break;
    case RC_RUNTIME_EVENT_LBOARD_STARTED:
      HandleLeaderboardStartedEvent(runtime_event);
      break;
    case RC_RUNTIME_EVENT_LBOARD_CANCELED:
      HandleLeaderboardCanceledEvent(runtime_event);
      break;
    case RC_RUNTIME_EVENT_LBOARD_TRIGGERED:
      HandleLeaderboardTriggeredEvent(runtime_event);
      break;
    }
  }
  if (m_update_callback)
    m_update_callback();
}

std::recursive_mutex* AchievementManager::GetLock()
{
  return &m_lock;
}

std::string AchievementManager::GetPlayerDisplayName() const
{
  return IsLoggedIn() ? m_display_name : "";
}

u32 AchievementManager::GetPlayerScore() const
{
  return IsLoggedIn() ? m_player_score : 0;
}

std::string AchievementManager::GetGameDisplayName() const
{
  return IsGameLoaded() ? m_game_data.title : "";
}

AchievementManager::PointSpread AchievementManager::TallyScore() const
{
  PointSpread spread{};
  if (!IsGameLoaded())
    return spread;
  for (const auto& entry : m_unlock_map)
  {
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

AchievementManager::UnlockStatus
AchievementManager::GetUnlockStatus(AchievementId achievement_id) const
{
  return m_unlock_map.at(achievement_id);
}

void AchievementManager::GetAchievementProgress(AchievementId achievement_id, u32* value,
                                                u32* target)
{
  rc_runtime_get_achievement_measured(&m_runtime, achievement_id, value, target);
}

AchievementManager::RichPresence AchievementManager::GetRichPresence()
{
  std::lock_guard lg{m_lock};
  RichPresence rich_presence = m_rich_presence;
  return rich_presence;
}

void AchievementManager::CloseGame()
{
  {
    std::lock_guard lg{m_lock};
    if (m_is_game_loaded)
    {
      m_is_game_loaded = false;
      ActivateDeactivateAchievements();
      ActivateDeactivateLeaderboards();
      ActivateDeactivateRichPresence();
      m_game_id = 0;
      m_unlock_map.clear();
      rc_api_destroy_fetch_game_data_response(&m_game_data);
      std::memset(&m_game_data, 0, sizeof(m_game_data));
      m_queue.Cancel();
      m_system = nullptr;
    }
  }
  if (m_update_callback)
    m_update_callback();
  INFO_LOG_FMT(ACHIEVEMENTS, "Game closed.");
}

void AchievementManager::Logout()
{
  CloseGame();
  Config::SetBaseOrCurrent(Config::RA_API_TOKEN, "");
  if (m_update_callback)
    m_update_callback();
  INFO_LOG_FMT(ACHIEVEMENTS, "Logged out from server.");
}

void AchievementManager::Shutdown()
{
  CloseGame();
  m_is_runtime_initialized = false;
  m_queue.Shutdown();
  // DON'T log out - keep those credentials for next run.
  rc_runtime_destroy(&m_runtime);
  INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager shut down.");
}

AchievementManager::ResponseType AchievementManager::VerifyCredentials(const std::string& password)
{
  rc_api_login_response_t login_data{};
  std::string username, api_token;
  {
    std::lock_guard lg{m_lock};
    username = Config::Get(Config::RA_USERNAME);
    api_token = Config::Get(Config::RA_API_TOKEN);
  }
  rc_api_login_request_t login_request = {
      .username = username.c_str(), .api_token = api_token.c_str(), .password = password.c_str()};
  ResponseType r_type = Request<rc_api_login_request_t, rc_api_login_response_t>(
      login_request, &login_data, rc_api_init_login_request, rc_api_process_login_response);
  if (r_type == ResponseType::SUCCESS)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Successfully logged in {} to RetroAchievements server.", username);
    std::lock_guard lg{m_lock};
    if (username != Config::Get(Config::RA_USERNAME))
    {
      INFO_LOG_FMT(ACHIEVEMENTS, "Attempted to login prior user {}; current user is {}.", username,
                   Config::Get(Config::RA_USERNAME));
      Config::SetBaseOrCurrent(Config::RA_API_TOKEN, "");
      return ResponseType::EXPIRED_CONTEXT;
    }
    Config::SetBaseOrCurrent(Config::RA_API_TOKEN, login_data.api_token);
    m_display_name = login_data.display_name;
    m_player_score = login_data.score;
  }
  else
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Failed to login {} to RetroAchievements server.", username);
  }
  rc_api_destroy_login_response(&login_data);
  return r_type;
}

AchievementManager::ResponseType
AchievementManager::ResolveHash(std::array<char, HASH_LENGTH> game_hash)
{
  rc_api_resolve_hash_response_t hash_data{};
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_resolve_hash_request_t resolve_hash_request = {
      .username = username.c_str(), .api_token = api_token.c_str(), .game_hash = game_hash.data()};
  ResponseType r_type = Request<rc_api_resolve_hash_request_t, rc_api_resolve_hash_response_t>(
      resolve_hash_request, &hash_data, rc_api_init_resolve_hash_request,
      rc_api_process_resolve_hash_response);
  if (r_type == ResponseType::SUCCESS)
  {
    m_game_id = hash_data.game_id;
    INFO_LOG_FMT(ACHIEVEMENTS, "Hashed game ID {} for RetroAchievements.", m_game_id);
  }
  else
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Hash {} not recognized by RetroAchievements.", game_hash.data());
  }
  rc_api_destroy_resolve_hash_response(&hash_data);
  return r_type;
}

AchievementManager::ResponseType AchievementManager::StartRASession()
{
  rc_api_start_session_response_t session_data{};
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_start_session_request_t start_session_request = {
      .username = username.c_str(), .api_token = api_token.c_str(), .game_id = m_game_id};
  ResponseType r_type = Request<rc_api_start_session_request_t, rc_api_start_session_response_t>(
      start_session_request, &session_data, rc_api_init_start_session_request,
      rc_api_process_start_session_response);
  rc_api_destroy_start_session_response(&session_data);
  return r_type;
}

AchievementManager::ResponseType AchievementManager::FetchGameData()
{
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_fetch_game_data_request_t fetch_data_request = {
      .username = username.c_str(), .api_token = api_token.c_str(), .game_id = m_game_id};
  return Request<rc_api_fetch_game_data_request_t, rc_api_fetch_game_data_response_t>(
      fetch_data_request, &m_game_data, rc_api_init_fetch_game_data_request,
      rc_api_process_fetch_game_data_response);
}

AchievementManager::ResponseType AchievementManager::FetchUnlockData(bool hardcore)
{
  rc_api_fetch_user_unlocks_response_t unlock_data{};
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_fetch_user_unlocks_request_t fetch_unlocks_request = {.username = username.c_str(),
                                                               .api_token = api_token.c_str(),
                                                               .game_id = m_game_id,
                                                               .hardcore = hardcore};
  ResponseType r_type =
      Request<rc_api_fetch_user_unlocks_request_t, rc_api_fetch_user_unlocks_response_t>(
          fetch_unlocks_request, &unlock_data, rc_api_init_fetch_user_unlocks_request,
          rc_api_process_fetch_user_unlocks_response);
  if (r_type == ResponseType::SUCCESS)
  {
    std::lock_guard lg{m_lock};
    bool enabled = Config::Get(Config::RA_ACHIEVEMENTS_ENABLED);
    bool unofficial = Config::Get(Config::RA_UNOFFICIAL_ENABLED);
    bool encore = Config::Get(Config::RA_ENCORE_ENABLED);
    for (AchievementId ix = 0; ix < unlock_data.num_achievement_ids; ix++)
    {
      auto it = m_unlock_map.find(unlock_data.achievement_ids[ix]);
      if (it == m_unlock_map.end())
        continue;
      it->second.remote_unlock_status =
          hardcore ? UnlockStatus::UnlockType::HARDCORE : UnlockStatus::UnlockType::SOFTCORE;
      ActivateDeactivateAchievement(unlock_data.achievement_ids[ix], enabled, unofficial, encore);
    }
  }
  rc_api_destroy_fetch_user_unlocks_response(&unlock_data);
  return r_type;
}

void AchievementManager::ActivateDeactivateAchievement(AchievementId id, bool enabled,
                                                       bool unofficial, bool encore)
{
  auto it = m_unlock_map.find(id);
  if (it == m_unlock_map.end())
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Attempted to unlock unknown achievement id {}.", id);
    return;
  }
  const UnlockStatus& status = it->second;
  u32 index = status.game_data_index;
  bool active = (rc_runtime_get_achievement(&m_runtime, id) != nullptr);

  // Deactivate achievements if game is not loaded
  bool activate = m_is_game_loaded;
  // Activate achievements only if achievements are enabled
  if (activate && !enabled)
    activate = false;
  // Deactivate if achievement is unofficial, unless unofficial achievements are enabled
  if (activate && !unofficial &&
      m_game_data.achievements[index].category == RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL)
  {
    activate = false;
  }
  // If encore mode is on, activate/deactivate regardless of current unlock status
  if (activate && !encore)
  {
    // Encore is off, achievement has been unlocked in this session, deactivate
    activate = (status.session_unlock_count == 0);
    // Encore is off, achievement has been hardcore unlocked on site, deactivate
    if (activate && status.remote_unlock_status == UnlockStatus::UnlockType::HARDCORE)
      activate = false;
    // Encore is off, hardcore is off, achievement has been softcore unlocked on site, deactivate
    if (activate && !hardcore_mode_enabled &&
        status.remote_unlock_status == UnlockStatus::UnlockType::SOFTCORE)
    {
      activate = false;
    }
  }

  if (!active && activate)
  {
    rc_runtime_activate_achievement(&m_runtime, id, m_game_data.achievements[index].definition,
                                    nullptr, 0);
  }
  if (active && !activate)
    rc_runtime_deactivate_achievement(&m_runtime, id);
}

void AchievementManager::GenerateRichPresence()
{
  Core::RunAsCPUThread([&] {
    std::lock_guard lg{m_lock};
    rc_runtime_get_richpresence(
        &m_runtime, m_rich_presence.data(), RP_SIZE,
        [](unsigned address, unsigned num_bytes, void* ud) {
          return static_cast<AchievementManager*>(ud)->MemoryPeeker(address, num_bytes, ud);
        },
        this, nullptr);
  });
}

AchievementManager::ResponseType AchievementManager::AwardAchievement(AchievementId achievement_id)
{
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_award_achievement_request_t award_request = {.username = username.c_str(),
                                                      .api_token = api_token.c_str(),
                                                      .achievement_id = achievement_id,
                                                      .hardcore = hardcore_mode_enabled,
                                                      .game_hash = m_game_hash.data()};
  rc_api_award_achievement_response_t award_response = {};
  ResponseType r_type =
      Request<rc_api_award_achievement_request_t, rc_api_award_achievement_response_t>(
          award_request, &award_response, rc_api_init_award_achievement_request,
          rc_api_process_award_achievement_response);
  rc_api_destroy_award_achievement_response(&award_response);
  if (r_type == ResponseType::SUCCESS)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Awarded achievement ID {}.", achievement_id);
  }
  else
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Failed to award achievement ID {}.", achievement_id);
  }
  return r_type;
}

AchievementManager::ResponseType AchievementManager::SubmitLeaderboard(AchievementId leaderboard_id,
                                                                       int value)
{
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_submit_lboard_entry_request_t submit_request = {.username = username.c_str(),
                                                         .api_token = api_token.c_str(),
                                                         .leaderboard_id = leaderboard_id,
                                                         .score = value,
                                                         .game_hash = m_game_hash.data()};
  rc_api_submit_lboard_entry_response_t submit_response = {};
  ResponseType r_type =
      Request<rc_api_submit_lboard_entry_request_t, rc_api_submit_lboard_entry_response_t>(
          submit_request, &submit_response, rc_api_init_submit_lboard_entry_request,
          rc_api_process_submit_lboard_entry_response);
  rc_api_destroy_submit_lboard_entry_response(&submit_response);
  if (r_type == ResponseType::SUCCESS)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Submitted leaderboard ID {}.", leaderboard_id);
  }
  else
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Failed to submit leaderboard ID {}.", leaderboard_id);
  }
  return r_type;
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

void AchievementManager::HandleAchievementTriggeredEvent(const rc_runtime_event_t* runtime_event)
{
  auto it = m_unlock_map.find(runtime_event->id);
  if (it == m_unlock_map.end())
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid achievement triggered event with id {}.",
                  runtime_event->id);
    return;
  }
  it->second.session_unlock_count++;
  m_queue.EmplaceItem([this, runtime_event] { AwardAchievement(runtime_event->id); });
  AchievementId game_data_index = it->second.game_data_index;
  OSD::AddMessage(fmt::format("Unlocked: {} ({})", m_game_data.achievements[game_data_index].title,
                              m_game_data.achievements[game_data_index].points),
                  OSD::Duration::VERY_LONG,
                  (hardcore_mode_enabled) ? OSD::Color::YELLOW : OSD::Color::CYAN);
  PointSpread spread = TallyScore();
  if (spread.hard_points == spread.total_points)
  {
    OSD::AddMessage(
        fmt::format("Congratulations! {} has mastered {}", m_display_name, m_game_data.title),
        OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
  }
  else if (spread.hard_points + spread.soft_points == spread.total_points)
  {
    OSD::AddMessage(
        fmt::format("Congratulations! {} has completed {}", m_display_name, m_game_data.title),
        OSD::Duration::VERY_LONG, OSD::Color::CYAN);
  }
  ActivateDeactivateAchievement(runtime_event->id, Config::Get(Config::RA_ACHIEVEMENTS_ENABLED),
                                Config::Get(Config::RA_UNOFFICIAL_ENABLED),
                                Config::Get(Config::RA_ENCORE_ENABLED));
}

void AchievementManager::HandleLeaderboardStartedEvent(const rc_runtime_event_t* runtime_event)
{
  for (u32 ix = 0; ix < m_game_data.num_leaderboards; ix++)
  {
    if (m_game_data.leaderboards[ix].id == runtime_event->id)
    {
      OSD::AddMessage(fmt::format("Attempting leaderboard: {}", m_game_data.leaderboards[ix].title),
                      OSD::Duration::VERY_LONG, OSD::Color::GREEN);
      return;
    }
  }
  ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid leaderboard started event with id {}.", runtime_event->id);
}

void AchievementManager::HandleLeaderboardCanceledEvent(const rc_runtime_event_t* runtime_event)
{
  for (u32 ix = 0; ix < m_game_data.num_leaderboards; ix++)
  {
    if (m_game_data.leaderboards[ix].id == runtime_event->id)
    {
      OSD::AddMessage(fmt::format("Failed leaderboard: {}", m_game_data.leaderboards[ix].title),
                      OSD::Duration::VERY_LONG, OSD::Color::RED);
      return;
    }
  }
  ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid leaderboard canceled event with id {}.", runtime_event->id);
}

void AchievementManager::HandleLeaderboardTriggeredEvent(const rc_runtime_event_t* runtime_event)
{
  m_queue.EmplaceItem(
      [this, runtime_event] { SubmitLeaderboard(runtime_event->id, runtime_event->value); });
  for (u32 ix = 0; ix < m_game_data.num_leaderboards; ix++)
  {
    if (m_game_data.leaderboards[ix].id == runtime_event->id)
    {
      FormattedValue value{};
      rc_runtime_format_lboard_value(value.data(), static_cast<int>(value.size()),
                                     runtime_event->value, m_game_data.leaderboards[ix].format);
      if (std::find(value.begin(), value.end(), '\0') == value.end())
      {
        OSD::AddMessage(fmt::format("Scored {} on leaderboard: {}",
                                    std::string_view{value.data(), value.size()},
                                    m_game_data.leaderboards[ix].title),
                        OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
      }
      else
      {
        OSD::AddMessage(fmt::format("Scored {} on leaderboard: {}", value.data(),
                                    m_game_data.leaderboards[ix].title),
                        OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
      }
      return;
    }
  }
  ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid leaderboard triggered event with id {}.", runtime_event->id);
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
    process_response(rc_response, response_str.c_str());
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
    WARN_LOG_FMT(ACHIEVEMENTS, "RetroAchievements connection failed.");
    return ResponseType::CONNECTION_FAILED;
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
