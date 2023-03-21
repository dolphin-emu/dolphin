// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS

#include "Core/AchievementManager.h"
#include "Common/HttpRequest.h"
#include "Common/WorkQueueThread.h"
#include "Config/AchievementSettings.h"
#include "Core/Core.h"

#ifdef ENABLE_RAINTEGRATION
#include <Windows.h>
#include <cstring>
#include "Common/scmrev.h"
#include "Core.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "RAInterface/RA_Consoles.h"
#include "RAInterface/RA_Interface.h"
#include "System.h"
#endif  // ENABLE_RAINTEGRATION

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

void AchievementManager::EnableDLL(bool enable)
{
  m_dll_enabled = enable;
}

bool AchievementManager::IsDLLEnabled()
{
  return m_dll_enabled;
}

#ifdef ENABLE_RAINTEGRATION
void AchievementManager::InitializeRAIntegration(void* main_window_handle)
{
  if (!m_dll_enabled)
    return;
  RA_InitClient((HWND)main_window_handle, "Dolphin", SCM_DESC_STR);
  RA_SetUserAgentDetail(std::format("Dolphin {} {}", SCM_DESC_STR, SCM_BRANCH_STR).c_str());

  RA_InstallSharedFunctions(RACallbackIsActive, RACallbackCauseUnpause, RACallbackCausePause,
                            RACallbackRebuildMenu, RACallbackEstimateTitle, RACallbackResetEmulator,
                            RACallbackLoadROM);

  // EE physical memory and scratchpad are currently exposed (matching direct rcheevos
  // implementation).
  m_memory_manager = &Core::System::GetInstance().GetMemory();
  ReinstallMemoryBanks();

  m_raintegration_initialized = true;

  RA_AttemptLogin(0);

  // this is pretty lame, but we may as well persist until we exit anyway
  std::atexit(RA_Shutdown);
}

void AchievementManager::ReinstallMemoryBanks()
{
  if (!m_dll_enabled)
    return;
  RA_ClearMemoryBanks();
  int memory_bank_size = 0;
  if (Core::GetState() != Core::State::Uninitialized)
  {
    memory_bank_size = m_memory_manager->GetExRamSizeReal();
  }
  RA_InstallMemoryBank(0, RACallbackReadMemory, RACallbackWriteMemory, memory_bank_size);
  RA_InstallMemoryBankBlockReader(0, RACallbackReadBlock);
}

void AchievementManager::MainWindowChanged(void* new_handle)
{
  if (!m_dll_enabled)
    return;
  if (m_raintegration_initialized)
  {
    RA_UpdateHWnd((HWND)new_handle);
    return;
  }

  InitializeRAIntegration(new_handle);
}

void AchievementManager::GameChanged(bool isWii)
{
  if (!m_dll_enabled)
    return;
  ReinstallMemoryBanks();
  if (Core::GetState() == Core::State::Uninitialized)
  {
    m_game_id = 0;
    return;
  }

  //  Must call this before calling RA_IdentifyHash
  RA_SetConsoleID(isWii ? WII : GameCube);

  m_game_id = RA_IdentifyHash(m_game_hash);
  if (m_game_id != 0)
  {
    RA_ActivateGame(m_game_id);
  }
}

void AchievementManager::RAIDoFrame()
{
  if (!m_dll_enabled)
    return;
  RA_DoAchievementsFrame();
}

bool WideStringToUTF8String(std::string& dest, const std::wstring_view& str)
{
  int mblen = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr,
                                  0, nullptr, nullptr);
  if (mblen < 0)
    return false;

  dest.resize(mblen);
  if (mblen > 0 && WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()),
                                       dest.data(), mblen, nullptr, nullptr) < 0)
  {
    return false;
  }

  return true;
}

std::string WideStringToUTF8String(const std::wstring_view& str)
{
  std::string ret;
  if (!WideStringToUTF8String(ret, str))
    ret.clear();

  return ret;
}

std::vector<std::tuple<int, std::string, bool>> AchievementManager::GetMenuItems()
{
  if (!m_dll_enabled)
    return std::vector<std::tuple<int, std::string, bool>>();
  std::array<RA_MenuItem, 64> items;
  const int num_items = RA_GetPopupMenuItems(items.data());

  std::vector<std::tuple<int, std::string, bool>> ret;
  ret.reserve(static_cast<u32>(num_items));

  for (int i = 0; i < num_items; i++)
  {
    const RA_MenuItem& it = items[i];
    if (!it.sLabel)
    {
      // separator
      ret.emplace_back(0, std::string(), false);
    }
    else
    {
      // option, maybe checkable
//      ret.emplace_back(static_cast<int>(it.nID), it.sLabel, it.bChecked);
      ret.emplace_back(static_cast<int>(it.nID), WideStringToUTF8String(it.sLabel), it.bChecked);
    }
  }

  return ret;
}

void AchievementManager::ActivateMenuItem(int item)
{
  if (!m_dll_enabled)
    return;
  RA_InvokeDialog(item);
}

int AchievementManager::RACallbackIsActive()
{
  return m_game_id;
}

void AchievementManager::RACallbackCauseUnpause()
{
  if (Core::GetState() != Core::State::Uninitialized)
    Core::SetState(Core::State::Running);
}

void AchievementManager::RACallbackCausePause()
{
  if (Core::GetState() != Core::State::Uninitialized)
    Core::SetState(Core::State::Paused);
}

void AchievementManager::RACallbackRebuildMenu()
{
  // unused
}

void AchievementManager::RACallbackEstimateTitle(char* buf)
{
  strcpy(buf, m_filename.c_str());
}

void AchievementManager::RACallbackResetEmulator()
{
  Core::Stop();
}

void AchievementManager::RACallbackLoadROM(const char* unused)
{
  // unused
}

unsigned char AchievementManager::RACallbackReadMemory(unsigned int address)
{
  return m_memory_manager->Read_U8(address);
}

unsigned int AchievementManager::RACallbackReadBlock(unsigned int address, unsigned char* buffer,
                                                     unsigned int bytes)
{
  m_memory_manager->CopyFromEmu(buffer, address, bytes);
  return bytes;
}

void AchievementManager::RACallbackWriteMemory(unsigned int address, unsigned char value)
{
  m_memory_manager->Write_U8(value, address);
}
#endif  // ENABLE_RAINTEGRATION
#endif  // USE_RETRO_ACHIEVEMENTS
