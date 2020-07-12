// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstddef>
#include <cstring>

#include "AudioCommon/CubebUtils.h"
#include "Common/CommonPaths.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"

#include <cubeb/cubeb.h>

static ptrdiff_t s_path_cutoff_point = 0;

// Due to the way Dolphin is, we can't guarantee a cubeb context will be created and destroyed
// from the same thread, this means that we can call CoInitialize on creation but not on destroy,
// as we don't want to CoUninitialize a random thread, so before calling DestroyContext(),
// we need to call CoInitialize ourselves. Every ScopedThreadAccess will be destroyed either with
// its context, if closed from the same thread, or when the thread ends
static thread_local std::unique_ptr<CubebUtils::ScopedThreadAccess> scoped_thread_accesses;

CubebUtils::ScopedThreadAccess::ScopedThreadAccess()
{
#ifdef _WIN32
  // COINIT_MULTITHREADED seems to be the better choice here given we handle multithread access
  // safely ourself and WASAPI is made for MTA. It should also perform better. Note that calls to
  // this from the main thread return RPC_E_CHANGED_MODE as some external library is calling CoInit
  // with COINIT_APARTMENTTHREADED without uninitializing it. We keep going. Cubeb specifies we need
  // COINIT_MULTITHREADED: https://github.com/kinetiknz/cubeb/issues/416 but that can't be a problem
  result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (result != RPC_E_CHANGED_MODE && FAILED(result))
    ERROR_LOG(AUDIO, "cubeb: Failed to initialize the COM library");
#endif
}
CubebUtils::ScopedThreadAccess::~ScopedThreadAccess()
{
#ifdef _WIN32
  if (SUCCEEDED(result))
    CoUninitialize();
#endif
}

bool CubebUtils::ScopedThreadAccess::Succeeded() const
{
#ifdef _WIN32
  return result == RPC_E_CHANGED_MODE || SUCCEEDED(result);
#else
  return true;
#endif
}

static void LogCallback(const char* format, ...)
{
  if (!Common::Log::LogManager::GetInstance())
    return;

  va_list args;
  va_start(args, format);

  const char* filename = va_arg(args, const char*) + s_path_cutoff_point;
  int lineno = va_arg(args, int);
  std::string adapted_format(StripSpaces(format + strlen("%s:%d:")));

  Common::Log::LogManager::GetInstance()->LogWithFullPath(
      Common::Log::LNOTICE, Common::Log::AUDIO, filename, lineno, adapted_format.c_str(), args);
  va_end(args);
}

static void DestroyContext(cubeb* ctx)
{
  cubeb_destroy(ctx);
  if (cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr) != CUBEB_OK)
  {
    ERROR_LOG(AUDIO, "Error removing cubeb log callback");
  }
  scoped_thread_accesses.reset();
}

// Initializes thread access for every call
std::shared_ptr<cubeb> CubebUtils::GetContext()
{
  static std::weak_ptr<cubeb> weak;

  if (!scoped_thread_accesses)
    scoped_thread_accesses = std::make_unique<CubebUtils::ScopedThreadAccess>();

  std::shared_ptr<cubeb> shared = weak.lock();
  // Already initialized
  if (shared)
    return shared;

  const char* filename = __FILE__;
  const char* match_point = strstr(filename, DIR_SEP "Source" DIR_SEP "Core" DIR_SEP);
  if (match_point)
  {
    s_path_cutoff_point = match_point - filename + strlen(DIR_SEP "Externals" DIR_SEP);
  }
  if (cubeb_set_log_callback(CUBEB_LOG_NORMAL, LogCallback) != CUBEB_OK)
  {
    ERROR_LOG(AUDIO, "Error setting cubeb log callback");
  }

  cubeb* ctx;
  if (cubeb_init(&ctx, "Dolphin", nullptr) != CUBEB_OK)
  {
    ERROR_LOG(AUDIO, "Error initializing cubeb library");
    scoped_thread_accesses.reset();
    return nullptr;
  }
  INFO_LOG(AUDIO, "Cubeb initialized using %s backend", cubeb_get_backend_id(ctx));

  weak = shared = {ctx, DestroyContext};
  return shared;
}
