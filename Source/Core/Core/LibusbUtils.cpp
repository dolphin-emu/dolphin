// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/LibusbUtils.h"

#include <mutex>
#include <thread>

#if defined(__LIBUSB__)
#include <libusb.h>
#endif

#include "Common/Assert.h"
#include "Common/Flag.h"
#include "Common/Thread.h"

namespace LibusbUtils
{
#if defined(__LIBUSB__)
class Context::Impl
{
public:
  Impl()
  {
    const int ret = libusb_init(&m_context);
    ASSERT_MSG(IOS_USB, ret == LIBUSB_SUCCESS, "Failed to init libusb: {}", ErrorWrap(ret));
    if (ret != LIBUSB_SUCCESS)
      return;

#ifdef _WIN32
    const int usbdk_ret = libusb_set_option(m_context, LIBUSB_OPTION_USE_USBDK);
    if (usbdk_ret != LIBUSB_SUCCESS && usbdk_ret != LIBUSB_ERROR_NOT_FOUND)
      WARN_LOG_FMT(IOS_USB, "Failed to set LIBUSB_OPTION_USE_USBDK: {}", ErrorWrap(usbdk_ret));
#endif
    m_event_thread_running.Set();
    m_event_thread = std::thread(&Impl::EventThread, this);
  }

  ~Impl()
  {
    if (!m_context || !m_event_thread_running.TestAndClear())
      return;
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000105
    libusb_interrupt_event_handler(m_context);
#endif
    m_event_thread.join();
    libusb_exit(m_context);
  }

  libusb_context* GetContext() const { return m_context; }

  int GetDeviceList(GetDeviceListCallback callback) const
  {
    std::lock_guard lock{m_device_list_mutex};

    libusb_device** list;
    ssize_t count = libusb_get_device_list(m_context, &list);
    if (count < 0)
      return static_cast<int>(count);

    for (ssize_t i = 0; i < count; ++i)
    {
      if (!callback(list[i]))
        break;
    }
    libusb_free_device_list(list, 1);
    return LIBUSB_SUCCESS;
  }

private:
  void EventThread()
  {
    Common::SetCurrentThreadName("libusb thread");
    timeval tv{5, 0};
    while (m_event_thread_running.IsSet())
    {
      const int ret = libusb_handle_events_timeout_completed(m_context, &tv, nullptr);
      if (ret != LIBUSB_SUCCESS)
        WARN_LOG_FMT(IOS_USB, "libusb_handle_events_timeout_completed failed: {}", ErrorWrap(ret));
    }
  }

  libusb_context* m_context = nullptr;
  mutable std::mutex m_device_list_mutex;
  Common::Flag m_event_thread_running;
  std::thread m_event_thread;
};
#else
class Context::Impl
{
public:
  libusb_context* GetContext() const { return nullptr; }
  int GetDeviceList(GetDeviceListCallback callback) const { return -1; }
};
#endif

Context::Context() : m_impl{std::make_unique<Impl>()}
{
}

Context::~Context() = default;

Context::operator libusb_context*() const
{
  return m_impl->GetContext();
}

bool Context::IsValid() const
{
  return m_impl->GetContext() != nullptr;
}

int Context::GetDeviceList(GetDeviceListCallback callback) const
{
  return m_impl->GetDeviceList(std::move(callback));
}

std::pair<int, ConfigDescriptor> MakeConfigDescriptor(libusb_device* device, u8 config_num)
{
#if defined(__LIBUSB__)
  libusb_config_descriptor* descriptor = nullptr;
  const int ret = libusb_get_config_descriptor(device, config_num, &descriptor);
  if (ret == LIBUSB_SUCCESS)
    return {ret, ConfigDescriptor{descriptor, libusb_free_config_descriptor}};
#else
  const int ret = -1;
#endif
  return {ret, ConfigDescriptor{nullptr, [](auto) {}}};
}

const char* ErrorWrap::GetName() const
{
#if defined(__LIBUSB__)
  return libusb_error_name(m_error);
#else
  return "__LIBUSB__ not defined";
#endif
}

const char* ErrorWrap::GetStrError() const
{
#if defined(__LIBUSB__)
  return libusb_strerror(static_cast<libusb_error>(m_error));
#else
  return "__LIBUSB__ not defined";
#endif
}
}  // namespace LibusbUtils
