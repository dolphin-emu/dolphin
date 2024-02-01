// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS

#include "Core/AchievementManager.h"

#include <memory>

#include <fmt/format.h>

#include <rcheevos/include/rc_api_info.h>
#include <rcheevos/include/rc_hash.h>

#include "Common/HttpRequest.h"
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

static constexpr bool hardcore_mode_enabled = false;

static std::unique_ptr<OSD::Icon> DecodeBadgeToOSDIcon(const AchievementManager::Badge& badge);

AchievementManager& AchievementManager::GetInstance()
{
  static AchievementManager s_instance;
  return s_instance;
}

void AchievementManager::Init()
{
  if (!m_is_runtime_initialized && Config::Get(Config::RA_ENABLED))
  {
    std::string host_url = Config::Get(Config::RA_HOST_URL);
    if (!host_url.empty())
      rc_api_set_host(host_url.c_str());
    rc_runtime_init(&m_runtime);
    m_is_runtime_initialized = true;
    m_queue.Reset("AchievementManagerQueue", [](const std::function<void()>& func) { func(); });
    m_image_queue.Reset("AchievementManagerImageQueue",
                        [](const std::function<void()>& func) { func(); });
    if (IsLoggedIn())
      LoginAsync("", [](ResponseType r_type) {});
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

AchievementManager::ResponseType AchievementManager::Login(const std::string& password)
{
  if (!m_is_runtime_initialized)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Attempted login (sync) to RetroAchievements server without "
                                "Achievement Manager initialized.");
    return ResponseType::MANAGER_NOT_INITIALIZED;
  }

  const ResponseType r_type = VerifyCredentials(password);
  FetchBadges();

  m_update_callback();
  return r_type;
}

void AchievementManager::LoginAsync(const std::string& password, const ResponseCallback& callback)
{
  if (!m_is_runtime_initialized)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Attempted login (async) to RetroAchievements server without "
                                "Achievement Manager initialized.");
    callback(ResponseType::MANAGER_NOT_INITIALIZED);
    return;
  }
  m_queue.EmplaceItem([this, password, callback] {
    callback(VerifyCredentials(password));
    FetchBadges();
    m_update_callback();
  });
}

bool AchievementManager::IsLoggedIn() const
{
  return !Config::Get(Config::RA_API_TOKEN).empty();
}

void AchievementManager::HashGame(const std::string& file_path, const ResponseCallback& callback)
{
  if (!Config::Get(Config::RA_ENABLED) || !IsLoggedIn())
  {
    callback(ResponseType::NOT_ENABLED);
    return;
  }
  if (!m_is_runtime_initialized)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS,
                  "Attempted to load game achievements without Achievement Manager initialized.");
    callback(ResponseType::MANAGER_NOT_INITIALIZED);
    return;
  }
  if (m_disabled)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager is disabled until core is rebooted.");
    OSD::AddMessage("Achievements are disabled until you restart emulation.",
                    OSD::Duration::VERY_LONG, OSD::Color::RED);
    return;
  }
  m_system = &Core::System::GetInstance();
  m_queue.EmplaceItem([this, callback, file_path] {
    Hash new_hash;
    {
      std::lock_guard lg{m_filereader_lock};
      rc_hash_filereader volume_reader{
          .open = &AchievementManager::FilereaderOpenByFilepath,
          .seek = &AchievementManager::FilereaderSeek,
          .tell = &AchievementManager::FilereaderTell,
          .read = &AchievementManager::FilereaderRead,
          .close = &AchievementManager::FilereaderClose,
      };
      rc_hash_init_custom_filereader(&volume_reader);
      if (!rc_hash_generate_from_file(new_hash.data(), RC_CONSOLE_GAMECUBE, file_path.c_str()))
      {
        ERROR_LOG_FMT(ACHIEVEMENTS, "Unable to generate achievement hash from game file {}.",
                      file_path);
        callback(ResponseType::MALFORMED_OBJECT);
      }
    }
    {
      std::lock_guard lg{m_lock};
      if (m_disabled)
      {
        INFO_LOG_FMT(ACHIEVEMENTS, "Achievements disabled while hash was resolving.");
        callback(ResponseType::EXPIRED_CONTEXT);
        return;
      }
      m_game_hash = std::move(new_hash);
    }
    LoadGameSync(callback);
  });
}

void AchievementManager::HashGame(const DiscIO::Volume* volume, const ResponseCallback& callback)
{
  if (!Config::Get(Config::RA_ENABLED) || !IsLoggedIn())
  {
    callback(ResponseType::NOT_ENABLED);
    return;
  }
  if (!m_is_runtime_initialized)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS,
                  "Attempted to load game achievements without Achievement Manager initialized.");
    callback(ResponseType::MANAGER_NOT_INITIALIZED);
    return;
  }
  if (volume == nullptr)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "New volume is empty.");
    return;
  }
  if (m_disabled)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager is disabled until core is rebooted.");
    OSD::AddMessage("Achievements are disabled until core is rebooted.", OSD::Duration::VERY_LONG,
                    OSD::Color::RED);
    return;
  }
  // Need to SetDisabled outside a lock because it uses m_lock internally.
  bool disable = true;
  {
    std::lock_guard lg{m_lock};
    if (!m_loading_volume)
    {
      m_loading_volume = DiscIO::CreateVolume(volume->GetBlobReader().CopyReader());
      disable = false;
    }
  }
  if (disable)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Disabling Achievement Manager due to hash spam.");
    SetDisabled(true);
    callback(ResponseType::EXPIRED_CONTEXT);
    return;
  }
  m_system = &Core::System::GetInstance();
  m_queue.EmplaceItem([this, callback] {
    Hash new_hash;
    {
      std::lock_guard lg{m_filereader_lock};
      rc_hash_filereader volume_reader{
          .open = &AchievementManager::FilereaderOpenByVolume,
          .seek = &AchievementManager::FilereaderSeek,
          .tell = &AchievementManager::FilereaderTell,
          .read = &AchievementManager::FilereaderRead,
          .close = &AchievementManager::FilereaderClose,
      };
      rc_hash_init_custom_filereader(&volume_reader);
      if (!rc_hash_generate_from_file(new_hash.data(), RC_CONSOLE_GAMECUBE, ""))
      {
        ERROR_LOG_FMT(ACHIEVEMENTS, "Unable to generate achievement hash from volume.");
        callback(ResponseType::MALFORMED_OBJECT);
        return;
      }
    }
    {
      std::lock_guard lg{m_lock};
      if (m_disabled)
      {
        INFO_LOG_FMT(ACHIEVEMENTS, "Achievements disabled while hash was resolving.");
        callback(ResponseType::EXPIRED_CONTEXT);
        return;
      }
      m_game_hash = std::move(new_hash);
      m_loading_volume.reset();
    }
    LoadGameSync(callback);
  });
}

void AchievementManager::LoadGameSync(const ResponseCallback& callback)
{
  if (!Config::Get(Config::RA_ENABLED) || !IsLoggedIn())
  {
    callback(ResponseType::NOT_ENABLED);
    return;
  }
  u32 new_game_id = 0;
  Hash current_hash;
  {
    std::lock_guard lg{m_lock};
    current_hash = m_game_hash;
  }
  const auto resolve_hash_response = ResolveHash(current_hash, &new_game_id);
  if (resolve_hash_response != ResponseType::SUCCESS || new_game_id == 0)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "No RetroAchievements data found for this game.");
    OSD::AddMessage("No RetroAchievements data found for this game.", OSD::Duration::VERY_LONG,
                    OSD::Color::RED);
    SetDisabled(true);
    callback(resolve_hash_response);
    return;
  }
  u32 old_game_id;
  {
    std::lock_guard lg{m_lock};
    old_game_id = m_game_id;
  }
  if (new_game_id == old_game_id)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Alternate hash resolved for current game {}.", old_game_id);
    callback(ResponseType::SUCCESS);
    return;
  }
  else if (old_game_id != 0)
  {
    INFO_LOG_FMT(ACHIEVEMENTS, "Swapping game {} for game {}; achievements disabled.", old_game_id,
                 new_game_id);
    OSD::AddMessage("Achievements are now disabled. Please close emulation to re-enable.",
                    OSD::Duration::VERY_LONG, OSD::Color::RED);
    SetDisabled(true);
    callback(ResponseType::EXPIRED_CONTEXT);
    return;
  }
  {
    std::lock_guard lg{m_lock};
    m_game_id = new_game_id;
  }

  const auto start_session_response = StartRASession();
  if (start_session_response != ResponseType::SUCCESS)
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "Failed to connect to RetroAchievements server.");
    OSD::AddMessage("Failed to connect to RetroAchievements server.", OSD::Duration::VERY_LONG,
                    OSD::Color::RED);
    callback(start_session_response);
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
    m_framecount = 0;
    LoadUnlockData([](ResponseType r_type) {});
    ActivateDeactivateAchievements();
    ActivateDeactivateLeaderboards();
    ActivateDeactivateRichPresence();
  }
  FetchBadges();
  // Reset this to zero so that RP immediately triggers on the first frame
  m_last_ping_time = 0;
  INFO_LOG_FMT(ACHIEVEMENTS, "RetroAchievements successfully loaded for {}.", m_game_data.title);

  m_update_callback();
  callback(fetch_game_data_response);
}

bool AchievementManager::IsGameLoaded() const
{
  return m_is_game_loaded;
}

void AchievementManager::LoadUnlockData(const ResponseCallback& callback)
{
  if (!Config::Get(Config::RA_ENABLED) || !IsLoggedIn())
  {
    callback(ResponseType::NOT_ENABLED);
    return;
  }
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
    m_update_callback();
  });
}

void AchievementManager::ActivateDeactivateAchievements()
{
  if (!Config::Get(Config::RA_ENABLED) || !IsLoggedIn())
    return;
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
  if (!Config::Get(Config::RA_ENABLED) || !IsLoggedIn())
    return;
  bool leaderboards_enabled = Config::Get(Config::RA_LEADERBOARDS_ENABLED);
  for (u32 ix = 0; ix < m_game_data.num_leaderboards; ix++)
  {
    auto leaderboard = m_game_data.leaderboards[ix];
    u32 leaderboard_id = leaderboard.id;
    if (m_is_game_loaded && leaderboards_enabled && hardcore_mode_enabled)
    {
      rc_runtime_activate_lboard(&m_runtime, leaderboard_id, leaderboard.definition, nullptr, 0);
      m_queue.EmplaceItem([this, leaderboard_id] {
        FetchBoardInfo(leaderboard_id);
        m_update_callback();
      });
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
  if (!Config::Get(Config::RA_ENABLED) || !IsLoggedIn())
    return;
  rc_runtime_activate_richpresence(
      &m_runtime,
      (m_is_game_loaded && Config::Get(Config::RA_RICH_PRESENCE_ENABLED)) ?
          m_game_data.rich_presence_script :
          "",
      nullptr, 0);
  INFO_LOG_FMT(ACHIEVEMENTS, "Rich presence (de)activated.");
}

void AchievementManager::FetchBadges()
{
  if (!m_is_runtime_initialized || !IsLoggedIn() || !Config::Get(Config::RA_BADGES_ENABLED))
  {
    m_update_callback();
    return;
  }
  m_image_queue.Cancel();

  if (m_player_badge.name != m_display_name)
  {
    m_image_queue.EmplaceItem([this] {
      std::string name_to_fetch;
      {
        std::lock_guard lg{m_lock};
        if (m_display_name == m_player_badge.name)
          return;
        name_to_fetch = m_display_name;
      }
      rc_api_fetch_image_request_t icon_request = {.image_name = name_to_fetch.c_str(),
                                                   .image_type = RC_IMAGE_TYPE_USER};
      Badge fetched_badge;
      if (RequestImage(icon_request, &fetched_badge) == ResponseType::SUCCESS)
      {
        INFO_LOG_FMT(ACHIEVEMENTS, "Successfully downloaded player badge id {}.", name_to_fetch);
        std::lock_guard lg{m_lock};
        if (name_to_fetch != m_display_name)
        {
          INFO_LOG_FMT(ACHIEVEMENTS, "Requested outdated badge id {} for player id {}.",
                       name_to_fetch, m_display_name);
          return;
        }
        m_player_badge.badge = std::move(fetched_badge);
        m_player_badge.name = std::move(name_to_fetch);
      }
      else
      {
        WARN_LOG_FMT(ACHIEVEMENTS, "Failed to download player badge id {}.", name_to_fetch);
      }

      m_update_callback();
    });
  }

  if (!IsGameLoaded())
  {
    m_update_callback();
    return;
  }

  bool badgematch = false;
  {
    std::lock_guard lg{m_lock};
    badgematch = m_game_badge.name == m_game_data.image_name;
  }
  if (!badgematch)
  {
    m_image_queue.EmplaceItem([this] {
      std::string name_to_fetch;
      {
        std::lock_guard lg{m_lock};
        if (m_game_badge.name == m_game_data.image_name)
          return;
        name_to_fetch = m_game_data.image_name;
      }
      rc_api_fetch_image_request_t icon_request = {.image_name = name_to_fetch.c_str(),
                                                   .image_type = RC_IMAGE_TYPE_GAME};
      Badge fetched_badge;
      if (RequestImage(icon_request, &fetched_badge) == ResponseType::SUCCESS)
      {
        INFO_LOG_FMT(ACHIEVEMENTS, "Successfully downloaded game badge id {}.", name_to_fetch);
        std::lock_guard lg{m_lock};
        if (name_to_fetch != m_game_data.image_name)
        {
          INFO_LOG_FMT(ACHIEVEMENTS, "Requested outdated badge id {} for game id {}.",
                       name_to_fetch, m_game_data.image_name);
          return;
        }
        m_game_badge.badge = std::move(fetched_badge);
        m_game_badge.name = std::move(name_to_fetch);
      }
      else
      {
        WARN_LOG_FMT(ACHIEVEMENTS, "Failed to download game badge id {}.", name_to_fetch);
      }

      m_update_callback();
    });
  }

  unsigned num_achievements = m_game_data.num_achievements;
  for (size_t index = 0; index < num_achievements; index++)
  {
    std::lock_guard lg{m_lock};

    // In case the number of achievements changes since the loop started; I just don't want
    // to lock for the ENTIRE loop so instead I reclaim the lock each cycle
    if (num_achievements != m_game_data.num_achievements)
      break;

    const auto& initial_achievement = m_game_data.achievements[index];
    const std::string badge_name_to_fetch(initial_achievement.badge_name);
    const UnlockStatus& unlock_status = m_unlock_map[initial_achievement.id];

    if (unlock_status.unlocked_badge.name != badge_name_to_fetch)
    {
      m_image_queue.EmplaceItem([this, index] {
        std::string current_name, name_to_fetch;
        {
          std::lock_guard lock{m_lock};
          if (m_game_data.num_achievements <= index)
          {
            INFO_LOG_FMT(
                ACHIEVEMENTS,
                "Attempted to fetch unlocked badge for index {} after achievement list cleared.",
                index);
            return;
          }
          const auto& achievement = m_game_data.achievements[index];
          const auto unlock_itr = m_unlock_map.find(achievement.id);
          if (unlock_itr == m_unlock_map.end())
          {
            ERROR_LOG_FMT(
                ACHIEVEMENTS,
                "Attempted to fetch unlocked badge for achievement id {} not in unlock map.",
                index);
            return;
          }
          name_to_fetch = achievement.badge_name;
          current_name = unlock_itr->second.unlocked_badge.name;
        }
        if (current_name == name_to_fetch)
          return;
        rc_api_fetch_image_request_t icon_request = {.image_name = name_to_fetch.c_str(),
                                                     .image_type = RC_IMAGE_TYPE_ACHIEVEMENT};
        Badge fetched_badge;
        if (RequestImage(icon_request, &fetched_badge) == ResponseType::SUCCESS)
        {
          INFO_LOG_FMT(ACHIEVEMENTS, "Successfully downloaded unlocked achievement badge id {}.",
                       name_to_fetch);
          std::lock_guard lock{m_lock};
          if (m_game_data.num_achievements <= index)
          {
            INFO_LOG_FMT(ACHIEVEMENTS,
                         "Fetched unlocked badge for index {} after achievement list cleared.",
                         index);
            return;
          }
          const auto& achievement = m_game_data.achievements[index];
          const auto unlock_itr = m_unlock_map.find(achievement.id);
          if (unlock_itr == m_unlock_map.end())
          {
            ERROR_LOG_FMT(ACHIEVEMENTS,
                          "Fetched unlocked badge for achievement id {} not in unlock map.", index);
            return;
          }
          if (name_to_fetch != achievement.badge_name)
          {
            INFO_LOG_FMT(
                ACHIEVEMENTS,
                "Requested outdated unlocked achievement badge id {} for achievement id {}.",
                name_to_fetch, current_name);
            return;
          }
          unlock_itr->second.unlocked_badge.badge = std::move(fetched_badge);
          unlock_itr->second.unlocked_badge.name = std::move(name_to_fetch);
        }
        else
        {
          WARN_LOG_FMT(ACHIEVEMENTS, "Failed to download unlocked achievement badge id {}.",
                       name_to_fetch);
        }

        m_update_callback();
      });
    }
    if (unlock_status.locked_badge.name != badge_name_to_fetch)
    {
      m_image_queue.EmplaceItem([this, index] {
        std::string current_name, name_to_fetch;
        {
          std::lock_guard lock{m_lock};
          if (m_game_data.num_achievements <= index)
          {
            INFO_LOG_FMT(
                ACHIEVEMENTS,
                "Attempted to fetch locked badge for index {} after achievement list cleared.",
                index);
            return;
          }
          const auto& achievement = m_game_data.achievements[index];
          const auto unlock_itr = m_unlock_map.find(achievement.id);
          if (unlock_itr == m_unlock_map.end())
          {
            ERROR_LOG_FMT(
                ACHIEVEMENTS,
                "Attempted to fetch locked badge for achievement id {} not in unlock map.", index);
            return;
          }
          name_to_fetch = achievement.badge_name;
          current_name = unlock_itr->second.locked_badge.name;
        }
        if (current_name == name_to_fetch)
          return;
        rc_api_fetch_image_request_t icon_request = {
            .image_name = name_to_fetch.c_str(), .image_type = RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED};
        Badge fetched_badge;
        if (RequestImage(icon_request, &fetched_badge) == ResponseType::SUCCESS)
        {
          INFO_LOG_FMT(ACHIEVEMENTS, "Successfully downloaded locked achievement badge id {}.",
                       name_to_fetch);
          std::lock_guard lock{m_lock};
          if (m_game_data.num_achievements <= index)
          {
            INFO_LOG_FMT(ACHIEVEMENTS,
                         "Fetched locked badge for index {} after achievement list cleared.",
                         index);
            return;
          }
          const auto& achievement = m_game_data.achievements[index];
          const auto unlock_itr = m_unlock_map.find(achievement.id);
          if (unlock_itr == m_unlock_map.end())
          {
            ERROR_LOG_FMT(ACHIEVEMENTS,
                          "Fetched locked badge for achievement id {} not in unlock map.", index);
            return;
          }
          if (name_to_fetch != achievement.badge_name)
          {
            INFO_LOG_FMT(ACHIEVEMENTS,
                         "Requested outdated locked achievement badge id {} for achievement id {}.",
                         name_to_fetch, current_name);
            return;
          }
          unlock_itr->second.locked_badge.badge = std::move(fetched_badge);
          unlock_itr->second.locked_badge.name = std::move(name_to_fetch);
        }
        else
        {
          WARN_LOG_FMT(ACHIEVEMENTS, "Failed to download locked achievement badge id {}.",
                       name_to_fetch);
        }

        m_update_callback();
      });
    }
  }

  m_update_callback();
}

void AchievementManager::DoFrame()
{
  if (!m_is_game_loaded)
    return;
  if (m_framecount == 0x200)
  {
    DisplayWelcomeMessage();
  }
  if (m_framecount <= 0x200)
  {
    m_framecount++;
  }
  Core::RunAsCPUThread([&] {
    rc_runtime_do_frame(
        &m_runtime,
        [](const rc_runtime_event_t* runtime_event) {
          GetInstance().AchievementEventHandler(runtime_event);
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
    return Common::swap16(
        m_system->GetMMU()
            .HostTryReadU16(threadguard, address, PowerPC::RequestedAddressSpace::Physical)
            .value_or(PowerPC::ReadResult<u16>(false, 0u))
            .value);
  case 4:
    return Common::swap32(
        m_system->GetMMU()
            .HostTryReadU32(threadguard, address, PowerPC::RequestedAddressSpace::Physical)
            .value_or(PowerPC::ReadResult<u32>(false, 0u))
            .value);
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
    case RC_RUNTIME_EVENT_ACHIEVEMENT_PROGRESS_UPDATED:
      HandleAchievementProgressUpdatedEvent(runtime_event);
      break;
    case RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED:
      HandleAchievementPrimedEvent(runtime_event);
      break;
    case RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED:
      HandleAchievementUnprimedEvent(runtime_event);
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

  m_update_callback();
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

std::string AchievementManager::GetPlayerDisplayName() const
{
  return IsLoggedIn() ? m_display_name : "";
}

u32 AchievementManager::GetPlayerScore() const
{
  return IsLoggedIn() ? m_player_score : 0;
}

const AchievementManager::BadgeStatus& AchievementManager::GetPlayerBadge() const
{
  return m_player_badge;
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
  }

  if (previously_disabled && !disable)
    INFO_LOG_FMT(ACHIEVEMENTS, "Achievement Manager has been re-enabled.");
};

const AchievementManager::NamedIconMap& AchievementManager::GetChallengeIcons() const
{
  return m_active_challenges;
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
      m_game_badge.name.clear();
      m_unlock_map.clear();
      m_leaderboard_map.clear();
      rc_api_destroy_fetch_game_data_response(&m_game_data);
      m_game_data = {};
      m_queue.Cancel();
      m_image_queue.Cancel();
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
    m_player_badge.name.clear();
    Config::SetBaseOrCurrent(Config::RA_API_TOKEN, "");
  }

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

AchievementManager::ResponseType AchievementManager::ResolveHash(const Hash& game_hash,
                                                                 u32* game_id)
{
  rc_api_resolve_hash_response_t hash_data{};
  std::string username, api_token;
  {
    std::lock_guard lg{m_lock};
    username = Config::Get(Config::RA_USERNAME);
    api_token = Config::Get(Config::RA_API_TOKEN);
  }
  rc_api_resolve_hash_request_t resolve_hash_request = {
      .username = username.c_str(), .api_token = api_token.c_str(), .game_hash = game_hash.data()};
  ResponseType r_type = Request<rc_api_resolve_hash_request_t, rc_api_resolve_hash_response_t>(
      resolve_hash_request, &hash_data, rc_api_init_resolve_hash_request,
      rc_api_process_resolve_hash_response);
  if (r_type == ResponseType::SUCCESS)
  {
    *game_id = hash_data.game_id;
    INFO_LOG_FMT(ACHIEVEMENTS, "Hashed game ID {} for RetroAchievements.", *game_id);
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
  rc_api_start_session_request_t start_session_request;
  rc_api_start_session_response_t session_data{};
  std::string username, api_token;
  {
    std::lock_guard lg{m_lock};
    username = Config::Get(Config::RA_USERNAME);
    api_token = Config::Get(Config::RA_API_TOKEN);
    start_session_request = {
        .username = username.c_str(), .api_token = api_token.c_str(), .game_id = m_game_id};
  }
  ResponseType r_type = Request<rc_api_start_session_request_t, rc_api_start_session_response_t>(
      start_session_request, &session_data, rc_api_init_start_session_request,
      rc_api_process_start_session_response);
  rc_api_destroy_start_session_response(&session_data);
  return r_type;
}

AchievementManager::ResponseType AchievementManager::FetchGameData()
{
  rc_api_fetch_game_data_request_t fetch_data_request;
  rc_api_request_t api_request;
  Common::HttpRequest http_request;
  std::string username, api_token;
  u32 game_id;
  {
    std::lock_guard lg{m_lock};
    username = Config::Get(Config::RA_USERNAME);
    api_token = Config::Get(Config::RA_API_TOKEN);
    game_id = m_game_id;
  }
  fetch_data_request = {
      .username = username.c_str(), .api_token = api_token.c_str(), .game_id = game_id};
  if (rc_api_init_fetch_game_data_request(&api_request, &fetch_data_request) != RC_OK ||
      !api_request.post_data)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid API request for game data.");
    return ResponseType::INVALID_REQUEST;
  }
  auto http_response = http_request.Post(api_request.url, api_request.post_data);
  rc_api_destroy_request(&api_request);
  if (!http_response.has_value() || http_response->size() == 0)
  {
    WARN_LOG_FMT(ACHIEVEMENTS,
                 "RetroAchievements connection failed while fetching game data for ID {}. \nURL: "
                 "{} \npost_data: {}",
                 game_id, api_request.url,
                 api_request.post_data == nullptr ? "NULL" : api_request.post_data);
    return ResponseType::CONNECTION_FAILED;
  }
  std::lock_guard lg{m_lock};
  const std::string response_str(http_response->begin(), http_response->end());
  if (rc_api_process_fetch_game_data_response(&m_game_data, response_str.c_str()) != RC_OK)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS,
                  "Failed to process HTTP response fetching game data for ID {}. \nURL: {} "
                  "\npost_data: {} \nresponse: {}",
                  game_id, api_request.url,
                  api_request.post_data == nullptr ? "NULL" : api_request.post_data, response_str);
    rc_api_destroy_fetch_game_data_response(&m_game_data);
    m_game_data = {};
    return ResponseType::MALFORMED_OBJECT;
  }
  if (!m_game_data.response.succeeded)
  {
    WARN_LOG_FMT(
        ACHIEVEMENTS,
        "Invalid RetroAchievements credentials fetching game data for ID {}; logging out user {}",
        game_id, username);
    // Logout technically does this via a CloseGame call, but doing this now prevents the activate
    // methods from thinking they have something to do.
    rc_api_destroy_fetch_game_data_response(&m_game_data);
    m_game_data = {};
    Logout();
    return ResponseType::INVALID_CREDENTIALS;
  }
  if (game_id != m_game_id)
  {
    INFO_LOG_FMT(ACHIEVEMENTS,
                 "Attempted to retrieve game data for ID {}; running game is now ID {}", game_id,
                 m_game_id);
    rc_api_destroy_fetch_game_data_response(&m_game_data);
    m_game_data = {};
    return ResponseType::EXPIRED_CONTEXT;
  }
  INFO_LOG_FMT(ACHIEVEMENTS, "Retrieved game data for ID {}.", game_id);
  return ResponseType::SUCCESS;
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

void AchievementManager::DisplayWelcomeMessage()
{
  std::lock_guard lg{m_lock};
  PointSpread spread = TallyScore();
  if (hardcore_mode_enabled)
  {
    OSD::AddMessage(
        fmt::format("You have {}/{} achievements worth {}/{} points", spread.hard_unlocks,
                    spread.total_count, spread.hard_points, spread.total_points),
        OSD::Duration::VERY_LONG, OSD::Color::YELLOW,
        (Config::Get(Config::RA_BADGES_ENABLED)) ? DecodeBadgeToOSDIcon(m_game_badge.badge) :
                                                   nullptr);
    OSD::AddMessage("Hardcore mode is ON", OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
  }
  else
  {
    OSD::AddMessage(fmt::format("You have {}/{} achievements worth {}/{} points",
                                spread.hard_unlocks + spread.soft_unlocks, spread.total_count,
                                spread.hard_points + spread.soft_points, spread.total_points),
                    OSD::Duration::VERY_LONG, OSD::Color::CYAN,
                    (Config::Get(Config::RA_BADGES_ENABLED)) ?
                        DecodeBadgeToOSDIcon(m_game_badge.badge) :
                        nullptr);
    OSD::AddMessage("Hardcore mode is OFF", OSD::Duration::VERY_LONG, OSD::Color::CYAN);
  }
}

void AchievementManager::HandleAchievementTriggeredEvent(const rc_runtime_event_t* runtime_event)
{
  const auto event_id = runtime_event->id;
  auto it = m_unlock_map.find(event_id);
  if (it == m_unlock_map.end())
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid achievement triggered event with id {}.", event_id);
    return;
  }
  it->second.session_unlock_count++;
  m_queue.EmplaceItem([this, event_id] { AwardAchievement(event_id); });
  AchievementId game_data_index = it->second.game_data_index;
  OSD::AddMessage(fmt::format("Unlocked: {} ({})", m_game_data.achievements[game_data_index].title,
                              m_game_data.achievements[game_data_index].points),
                  OSD::Duration::VERY_LONG,
                  (hardcore_mode_enabled) ? OSD::Color::YELLOW : OSD::Color::CYAN,
                  (Config::Get(Config::RA_BADGES_ENABLED)) ?
                      DecodeBadgeToOSDIcon(it->second.unlocked_badge.badge) :
                      nullptr);
  PointSpread spread = TallyScore();
  if (spread.hard_points == spread.total_points)
  {
    OSD::AddMessage(
        fmt::format("Congratulations! {} has mastered {}", m_display_name, m_game_data.title),
        OSD::Duration::VERY_LONG, OSD::Color::YELLOW,
        (Config::Get(Config::RA_BADGES_ENABLED)) ? DecodeBadgeToOSDIcon(m_game_badge.badge) :
                                                   nullptr);
  }
  else if (spread.hard_points + spread.soft_points == spread.total_points)
  {
    OSD::AddMessage(
        fmt::format("Congratulations! {} has completed {}", m_display_name, m_game_data.title),
        OSD::Duration::VERY_LONG, OSD::Color::CYAN,
        (Config::Get(Config::RA_BADGES_ENABLED)) ? DecodeBadgeToOSDIcon(m_game_badge.badge) :
                                                   nullptr);
  }
  ActivateDeactivateAchievement(event_id, Config::Get(Config::RA_ACHIEVEMENTS_ENABLED),
                                Config::Get(Config::RA_UNOFFICIAL_ENABLED),
                                Config::Get(Config::RA_ENCORE_ENABLED));
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
      OSD::Duration::VERY_LONG, OSD::Color::GREEN,
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
  const auto event_id = runtime_event->id;
  const auto event_value = runtime_event->value;
  m_queue.EmplaceItem([this, event_id, event_value] { SubmitLeaderboard(event_id, event_value); });
  for (u32 ix = 0; ix < m_game_data.num_leaderboards; ix++)
  {
    if (m_game_data.leaderboards[ix].id == event_id)
    {
      FormattedValue value{};
      rc_runtime_format_lboard_value(value.data(), static_cast<int>(value.size()), event_value,
                                     m_game_data.leaderboards[ix].format);
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
      m_queue.EmplaceItem([this, event_id] {
        FetchBoardInfo(event_id);
        m_update_callback();
      });
      break;
    }
  }
  ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid leaderboard triggered event with id {}.", event_id);
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

AchievementManager::ResponseType
AchievementManager::RequestImage(rc_api_fetch_image_request_t rc_request, Badge* rc_response)
{
  rc_api_request_t api_request;
  Common::HttpRequest http_request;
  if (rc_api_init_fetch_image_request(&api_request, &rc_request) != RC_OK)
  {
    ERROR_LOG_FMT(ACHIEVEMENTS, "Invalid request for image.");
    return ResponseType::INVALID_REQUEST;
  }
  auto http_response = http_request.Get(api_request.url);
  if (http_response.has_value() && http_response->size() > 0)
  {
    rc_api_destroy_request(&api_request);
    *rc_response = std::move(*http_response);
    return ResponseType::SUCCESS;
  }
  else
  {
    WARN_LOG_FMT(ACHIEVEMENTS, "RetroAchievements connection failed on image request.\n URL: {}",
                 api_request.url);
    rc_api_destroy_request(&api_request);
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

#endif  // USE_RETRO_ACHIEVEMENTS
