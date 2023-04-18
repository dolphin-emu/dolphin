// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS

#include "Core/AchievementManager.h"

#include <array>
#include <rcheevos/include/rc_hash.h>

#include "Common/HttpRequest.h"
#include "Common/WorkQueueThread.h"
#include "Config/AchievementSettings.h"
#include "Core/Core.h"
#include "DiscIO/Volume.h"

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
  }
}

AchievementManager::ResponseType AchievementManager::Login(const std::string& password)
{
  if (!m_is_runtime_initialized)
    return AchievementManager::ResponseType::MANAGER_NOT_INITIALIZED;
  return VerifyCredentials(password);
}

void AchievementManager::LoginAsync(const std::string& password, const ResponseCallback& callback)
{
  if (!m_is_runtime_initialized)
  {
    callback(AchievementManager::ResponseType::MANAGER_NOT_INITIALIZED);
    return;
  }
  m_queue.EmplaceItem([this, password, callback] { callback(VerifyCredentials(password)); });
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
    callback(AchievementManager::ResponseType::MANAGER_NOT_INITIALIZED);
    return;
  }
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
  std::array<char, HASH_LENGTH> game_hash;
  if (!rc_hash_generate_from_file(game_hash.data(), RC_CONSOLE_GAMECUBE, iso_path.c_str()))
    return;
  m_queue.EmplaceItem([this, callback, game_hash] {
    const auto resolve_hash_response = ResolveHash(game_hash);
    if (resolve_hash_response != ResponseType::SUCCESS || m_game_id == 0)
    {
      callback(resolve_hash_response);
      return;
    }

    const auto start_session_response = StartRASession();
    if (start_session_response != ResponseType::SUCCESS)
    {
      callback(start_session_response);
      return;
    }

    const auto fetch_game_data_response = FetchGameData();
    m_is_game_loaded = fetch_game_data_response == ResponseType::SUCCESS;
    callback(fetch_game_data_response);
  });
}

void AchievementManager::CloseGame()
{
  m_is_game_loaded = false;
  m_game_id = 0;
  m_queue.Cancel();
}

void AchievementManager::Logout()
{
  CloseGame();
  Config::SetBaseOrCurrent(Config::RA_API_TOKEN, "");
}

void AchievementManager::Shutdown()
{
  CloseGame();
  m_is_runtime_initialized = false;
  m_queue.Shutdown();
  // DON'T log out - keep those credentials for next run.
  rc_runtime_destroy(&m_runtime);
}

AchievementManager::ResponseType AchievementManager::VerifyCredentials(const std::string& password)
{
  rc_api_login_response_t login_data{};
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_login_request_t login_request = {
      .username = username.c_str(), .api_token = api_token.c_str(), .password = password.c_str()};
  ResponseType r_type = Request<rc_api_login_request_t, rc_api_login_response_t>(
      login_request, &login_data, rc_api_init_login_request, rc_api_process_login_response);
  if (r_type == ResponseType::SUCCESS)
    Config::SetBaseOrCurrent(Config::RA_API_TOKEN, login_data.api_token);
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
    m_game_id = hash_data.game_id;
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
  init_request(&api_request, &rc_request);
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
      return ResponseType::INVALID_CREDENTIALS;
    }
  }
  else
  {
    return ResponseType::CONNECTION_FAILED;
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
