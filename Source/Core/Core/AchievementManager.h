// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include <rcheevos/include/rc_runtime.h>

#include "Common/Event.h"
#include "Common/WorkQueueThread.h"

class AchievementManager
{
public:
  enum class ResponseType
  {
    SUCCESS,
    MANAGER_NOT_INITIALIZED,
    INVALID_CREDENTIALS,
    CONNECTION_FAILED,
    UNKNOWN_FAILURE
  };
  using ResponseCallback = std::function<void(ResponseType)>;

  static AchievementManager* GetInstance();
  void Init();
  ResponseType Login(const std::string& password);
  void LoginAsync(const std::string& password, const ResponseCallback& callback);
  bool IsLoggedIn() const;
  void LoadGameByFilenameAsync(const std::string& iso_path, const ResponseCallback& callback);

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

  template <typename RcRequest, typename RcResponse>
  ResponseType Request(RcRequest rc_request, RcResponse* rc_response,
                       const std::function<int(rc_api_request_t*, const RcRequest*)>& init_request,
                       const std::function<int(RcResponse*, const char*)>& process_response);

  rc_runtime_t m_runtime{};
  bool m_is_runtime_initialized = false;
  unsigned int m_game_id = 0;
  rc_api_fetch_game_data_response_t m_game_data{};
  bool m_is_game_loaded = false;

  Common::WorkQueueThread<std::function<void()>> m_queue;
};  // class AchievementManager

#endif  // USE_RETRO_ACHIEVEMENTS
