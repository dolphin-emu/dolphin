// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <rcheevos/include/rc_api_user.h>
#include <rcheevos/include/rc_runtime.h>

#include "Common/Event.h"
#include "Common/WorkQueueThread.h"
#include "Core/HW/Memmap.h"

const int HASH_LENGTH = 33;

class AchievementManager
{
public:
  enum class ResponseType
  {
    SUCCESS,
    INVALID_CREDENTIALS,
    CONNECTION_FAILED,
    UNKNOWN_FAILURE
  };
  using LoginCallback = std::function<void(ResponseType)>;

  static AchievementManager* GetInstance();
  void Init();
  ResponseType Login(const std::string& password);
  void LoginAsync(const std::string& password, const LoginCallback& callback);
  bool IsLoggedIn() const;
  void Logout();
  void Shutdown();

  void EnableDLL(bool enable);
  bool IsDLLEnabled();

#ifdef ENABLE_RAINTEGRATION
  void ReinstallMemoryBanks();
  void MainWindowChanged(void* new_handle);
  void GameChanged(bool isWii);
  void RAIDoFrame();
  std::vector<std::tuple<int, std::string, bool>> GetMenuItems();
  void ActivateMenuItem(int item);
#endif  // ENABLE_RAINTEGRATION

private:
  AchievementManager() = default;

  ResponseType VerifyCredentials(const std::string& password);

  template <typename RcRequest, typename RcResponse>
  ResponseType Request(RcRequest rc_request, RcResponse* rc_response,
                       const std::function<int(rc_api_request_t*, const RcRequest*)>& init_request,
                       const std::function<int(RcResponse*, const char*)>& process_response);

#ifdef ENABLE_RAINTEGRATION
  void InitializeRAIntegration(void* main_window_handle);
  static int RACallbackIsActive();
  static void RACallbackCauseUnpause();
  static void RACallbackCausePause();
  static void RACallbackRebuildMenu();
  static void RACallbackEstimateTitle(char* buf);
  static void RACallbackResetEmulator();
  static void RACallbackLoadROM(const char* unused);
  static unsigned char RACallbackReadMemory(unsigned int address);
  static unsigned int RACallbackReadBlock(unsigned int address, unsigned char* buffer,
                                          unsigned int bytes);
  static void RACallbackWriteMemory(unsigned int address, unsigned char value);
#endif  // ENABLE_RAINTEGRATION

  rc_runtime_t m_runtime{};
  bool m_is_runtime_initialized = false;
  rc_api_login_response_t m_login_data{};
  Common::WorkQueueThread<std::function<void()>> m_queue;
  bool m_dll_enabled = false;
  static std::string m_filename;
  char m_game_hash[HASH_LENGTH];
  static int m_game_id;
  static Memory::MemoryManager* m_memory_manager;

#ifdef ENABLE_RAINTEGRATION
  bool m_raintegration_initialized = false;
#endif  // ENABLE_RAINTEGRATION
};  // class AchievementManager

#endif  // USE_RETRO_ACHIEVEMENTS
