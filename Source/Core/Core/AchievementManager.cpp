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
  return VerifyCredentials(password);
}

void AchievementManager::LoginAsync(const std::string& password, const LoginCallback& callback)
{
  m_queue.EmplaceItem([this, password, callback] { callback(VerifyCredentials(password)); });
}

bool AchievementManager::IsLoggedIn() const
{
  return m_login_data.response.succeeded;
}

void AchievementManager::Logout()
{
  Config::SetBaseOrCurrent(Config::RA_API_TOKEN, "");
  rc_api_destroy_login_response(&m_login_data);
  m_login_data.response.succeeded = 0;
}

void AchievementManager::Shutdown()
{
  m_is_runtime_initialized = false;
  m_queue.Shutdown();
  // DON'T log out - keep those credentials for next run.
  rc_api_destroy_login_response(&m_login_data);
  m_login_data.response.succeeded = 0;
  rc_runtime_destroy(&m_runtime);
}

AchievementManager::ResponseType AchievementManager::VerifyCredentials(const std::string& password)
{
  std::string username = Config::Get(Config::RA_USERNAME);
  std::string api_token = Config::Get(Config::RA_API_TOKEN);
  rc_api_login_request_t login_request = {
      .username = username.c_str(), .api_token = api_token.c_str(), .password = password.c_str()};
  ResponseType r_type = Request<rc_api_login_request_t, rc_api_login_response_t>(
      login_request, &m_login_data, rc_api_init_login_request, rc_api_process_login_response);
  if (r_type == ResponseType::SUCCESS)
    Config::SetBaseOrCurrent(Config::RA_API_TOKEN, m_login_data.api_token);
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
