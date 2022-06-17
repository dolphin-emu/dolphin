// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/CubebUtils.h"

#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <thread>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"

#include <cubeb/cubeb.h>

#ifdef _WIN32
#include <Objbase.h>
#endif

// On Windows, we must manually ensure that COM is initialized in MTA mode on every thread that
// accesses the cubeb API. See the comment on cubeb_init in cubeb.h
// We do this with a thread-local variable that keeps track of whether COM is initialized or not,
// and initialize it if it isn't. When the thread ends COM is uninitialized again.
#ifdef _WIN32
namespace
{
class auto_com
{
public:
  auto_com() = default;
  auto_com(const auto_com&) = delete;
  auto_com(auto_com&&) = delete;
  auto_com& operator=(const auto_com&) = delete;
  auto_com& operator=(auto_com&&) = delete;
  ~auto_com()
  {
    if (m_initialized)
    {
      CoUninitialize();
    }
  }
  bool initialize()
  {
    if (!m_initialized)
    {
      HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
      m_initialized = SUCCEEDED(result);
    }
    return m_initialized;
  }

private:
  bool m_initialized = false;
};
}  // namespace
static thread_local auto_com tls_com_context;
#endif

static ptrdiff_t s_path_cutoff_point = 0;

static void LogCallback(const char* format, ...)
{
  auto* instance = Common::Log::LogManager::GetInstance();
  if (instance == nullptr)
    return;

  constexpr auto log_type = Common::Log::LogType::AUDIO;
  if (!instance->IsEnabled(log_type))
    return;

  va_list args;
  va_start(args, format);
  const char* filename = va_arg(args, const char*) + s_path_cutoff_point;
  const int lineno = va_arg(args, int);
  const std::string adapted_format(StripWhitespace(format + strlen("%s:%d:")));
  const std::string message = StringFromFormatV(adapted_format.c_str(), args);
  va_end(args);

  instance->LogWithFullPath(Common::Log::LogLevel::LNOTICE, log_type, filename, lineno,
                            message.c_str());
}

static void DestroyContext(cubeb* ctx)
{
  cubeb_destroy(ctx);
  if (cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr) != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error removing cubeb log callback");
  }
}

static bool EnsureCubebCallable()
{
#ifdef _WIN32
  if (!tls_com_context.initialize())
    return false;
#endif
  return true;
}

void CubebUtils::RunInCubebContext(const std::function<void()>& func)
{
  // Cubeb is documented to require MTA COM mode, so if the current thread was initialized in STA
  // mode, we make a temporary thread to execute the cubeb call.
  if (EnsureCubebCallable())
  {
    func();
  }
  else
  {
    std::thread([&] {
      // this should never fail, so yell loudly if it does
      ASSERT(EnsureCubebCallable());
      func();
    }).join();
  }
}

std::shared_ptr<cubeb> CubebUtils::GetContext()
{
  if (!EnsureCubebCallable())
    return nullptr;

  static std::weak_ptr<cubeb> weak;

  std::shared_ptr<cubeb> shared = weak.lock();
  // Already initialized
  if (shared)
    return shared;

  const char* filename = __FILE__;
  const char* match_point = strstr(filename, DIR_SEP "Source" DIR_SEP "Core" DIR_SEP);
  if (!match_point)
    match_point = strstr(filename, R"(\Source\Core\)");
  if (match_point)
  {
    s_path_cutoff_point = match_point - filename + strlen(DIR_SEP "Externals" DIR_SEP);
  }
  if (cubeb_set_log_callback(CUBEB_LOG_NORMAL, LogCallback) != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error setting cubeb log callback");
  }

  cubeb* ctx;
  if (cubeb_init(&ctx, "Dolphin", nullptr) != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error initializing cubeb library");
    return nullptr;
  }
  INFO_LOG_FMT(AUDIO, "Cubeb initialized using {} backend", cubeb_get_backend_id(ctx));

  weak = shared = {ctx, DestroyContext};
  return shared;
}
