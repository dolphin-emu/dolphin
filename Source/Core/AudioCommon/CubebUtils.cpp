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

#ifdef _WIN32
#include <Objbase.h>
#endif

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

namespace CubebUtils
{
std::shared_ptr<cubeb> GetContext()
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

std::vector<std::pair<std::string, std::string>> ListInputDevices()
{
  std::vector<std::pair<std::string, std::string>> devices;

  cubeb_device_collection collection;
  auto cubeb_ctx = GetContext();
  const int r = cubeb_enumerate_devices(cubeb_ctx.get(), CUBEB_DEVICE_TYPE_INPUT, &collection);

  if (r != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error listing cubeb input devices");
    return devices;
  }

  INFO_LOG_FMT(AUDIO, "Listing cubeb input devices:");
  for (uint32_t i = 0; i < collection.count; i++)
  {
    const auto& info = collection.device[i];
    const auto device_state = info.state;
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

    // According to cubeb_device_info definition in cubeb.h:
    //  > "Optional vendor name, may be NULL."
    // In practice, it seems some other fields might be NULL as well.
    static constexpr auto fmt_str = [](const char* ptr) constexpr -> const char* {
      return (ptr == nullptr) ? "(null)" : ptr;
    };

    INFO_LOG_FMT(AUDIO,
                 "[{}] Device ID: {}\n"
                 "\tName: {}\n"
                 "\tGroup ID: {}\n"
                 "\tVendor: {}\n"
                 "\tState: {}",
                 i, fmt_str(info.device_id), fmt_str(info.friendly_name), fmt_str(info.group_id),
                 fmt_str(info.vendor_name), state_name);

    if (info.device_id == nullptr)
      continue;  // Shouldn't happen

    if (info.state == CUBEB_DEVICE_STATE_ENABLED)
    {
      devices.emplace_back(info.device_id, fmt_str(info.friendly_name));
    }
  }

  cubeb_device_collection_destroy(cubeb_ctx.get(), &collection);

  return devices;
}

cubeb_devid GetInputDeviceById(std::string_view id)
{
  if (id.empty())
    return nullptr;

  cubeb_device_collection collection;
  auto cubeb_ctx = CubebUtils::GetContext();
  const int r = cubeb_enumerate_devices(cubeb_ctx.get(), CUBEB_DEVICE_TYPE_INPUT, &collection);

  if (r != CUBEB_OK)
  {
    ERROR_LOG_FMT(AUDIO, "Error enumerating cubeb input devices");
    return nullptr;
  }

  cubeb_devid device_id = nullptr;
  for (uint32_t i = 0; i < collection.count; i++)
  {
    const auto& info = collection.device[i];
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

CoInitSyncWorker::CoInitSyncWorker([[maybe_unused]] std::string worker_name)
#ifdef _WIN32
    : m_work_queue{std::move(worker_name)}
#endif
{
#ifdef _WIN32
  m_work_queue.PushBlocking([this] {
    const auto result = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    m_coinit_success = result == S_OK;
    m_should_couninit = m_coinit_success || result == S_FALSE;
  });
#endif
}

CoInitSyncWorker::~CoInitSyncWorker()
{
#ifdef _WIN32
  if (m_should_couninit)
  {
    m_work_queue.PushBlocking([this] {
      m_should_couninit = false;
      CoUninitialize();
    });
  }
  m_coinit_success = false;
#endif
}

bool CoInitSyncWorker::Execute(FunctionType f)
{
#ifdef _WIN32
  if (!m_coinit_success)
    return false;

  m_work_queue.PushBlocking(f);
#else
  f();
#endif
  return true;
}
}  // namespace CubebUtils
