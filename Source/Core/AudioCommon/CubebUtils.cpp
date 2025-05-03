// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/CubebUtils.h"

#include <cstdarg>
#include <cstring>
#include <string_view>

#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"

#include <cubeb/cubeb.h>

static void LogCallback(const char* format, ...)
{
  auto* instance = Common::Log::LogManager::GetInstance();
  if (instance == nullptr)
    return;

  constexpr auto log_type = Common::Log::LogType::AUDIO;
  constexpr auto log_level = Common::Log::LogLevel::LINFO;
  if (!instance->IsEnabled(log_type, log_level))
    return;

  va_list args;
  va_start(args, format);
  const char* filename = va_arg(args, const char*);
  const auto last_slash = std::string_view(filename).find_last_of("/\\");
  if (last_slash != std::string_view::npos)
    filename = filename + last_slash + 1;
  const int lineno = va_arg(args, int);
  const std::string adapted_format(StripWhitespace(format + strlen("%s:%d:")));
  const std::string message = StringFromFormatV(adapted_format.c_str(), args);
  va_end(args);

  instance->LogWithFullPath(log_level, log_type, filename, lineno, message.c_str());
}

static void DestroyContext(cubeb* ctx)
{
  cubeb_destroy(ctx);
  if (cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr) != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error removing cubeb log callback");
  }
}

std::shared_ptr<cubeb> CubebUtils::GetContext()
{
  static std::weak_ptr<cubeb> weak;

  std::shared_ptr<cubeb> shared = weak.lock();
  // Already initialized
  if (shared)
    return shared;

  if (cubeb_set_log_callback(CUBEB_LOG_NORMAL, LogCallback) != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error setting cubeb log callback");
  }

  cubeb* ctx;
  if (cubeb_init(&ctx, "Dolphin Emulator", nullptr) != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error initializing cubeb library");
    return nullptr;
  }
  INFO_LOG_FMT(AUDIO, "Cubeb initialized using {} backend", cubeb_get_backend_id(ctx));

  weak = shared = {ctx, DestroyContext};
  return shared;
}
