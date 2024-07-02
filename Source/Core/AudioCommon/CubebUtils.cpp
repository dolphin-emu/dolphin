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

static ptrdiff_t s_path_cutoff_point = 0;

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
  const char* filename = va_arg(args, const char*) + s_path_cutoff_point;
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
  if (cubeb_init(&ctx, "Dolphin Emulator", nullptr) != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error initializing cubeb library");
    return nullptr;
  }
  INFO_LOG_FMT(AUDIO, "Cubeb initialized using {} backend", cubeb_get_backend_id(ctx));

  weak = shared = {ctx, DestroyContext};
  return shared;
}

std::vector<std::pair<std::string, std::string>> CubebUtils::ListInputDevices()
{
  std::vector<std::pair<std::string, std::string>> devices;

  cubeb_device_collection collection;
  auto cubeb_ctx = CubebUtils::GetContext();
  int r = cubeb_enumerate_devices(cubeb_ctx.get(), CUBEB_DEVICE_TYPE_INPUT, &collection);

  if (r != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error listing cubeb input devices");
    return devices;
  }

  INFO_LOG_FMT(AUDIO, "Listing cubeb input devices:");
  for (uint32_t i = 0; i < collection.count; i++)
  {
    auto& info = collection.device[i];
    auto& device_state = info.state;
    const char* state_name = [device_state] {
      switch (device_state)
      {
      case CUBEB_DEVICE_STATE_DISABLED:
        return "disabled";
      case CUBEB_DEVICE_STATE_UNPLUGGED:
        return "unplugged";
      case CUBEB_DEVICE_STATE_ENABLED:
        return "enabled";
      default:
        return "unknown?";
      }
    }();

    INFO_LOG_FMT(AUDIO,
                 "[{}] Device ID: {}\n"
                 "\tName: {}\n"
                 "\tGroup ID: {}\n"
                 "\tVendor: {}\n"
                 "\tState: {}",
                 i, info.device_id, info.friendly_name, info.group_id,
                 (info.vendor_name == nullptr) ? "(null)" : info.vendor_name, state_name);
    if (info.state == CUBEB_DEVICE_STATE_ENABLED)
    {
      devices.emplace_back(info.device_id, info.friendly_name);
    }
  }

  cubeb_device_collection_destroy(cubeb_ctx.get(), &collection);

  return devices;
}

cubeb_devid CubebUtils::GetInputDeviceById(const std::string& id)
{
  if (id.empty())
    return nullptr;

  cubeb_device_collection collection;
  auto cubeb_ctx = CubebUtils::GetContext();
  int r = cubeb_enumerate_devices(cubeb_ctx.get(), CUBEB_DEVICE_TYPE_INPUT, &collection);

  if (r != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error enumerating cubeb input devices");
    return nullptr;
  }

  cubeb_devid device_id = nullptr;
  for (uint32_t i = 0; i < collection.count; i++)
  {
    auto& info = collection.device[i];
    if (id.compare(info.device_id) == 0)
    {
      device_id = info.devid;
      break;
    }
  }
  if (device_id == nullptr)
  {
    WARN_LOG_FMT(AUDIO, "Failed to find selected input device, defaulting to system preferences");
  }

  cubeb_device_collection_destroy(cubeb_ctx.get(), &collection);

  return device_id;
}
