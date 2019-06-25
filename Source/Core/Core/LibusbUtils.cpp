// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <mutex>
#include <thread>

#if defined(__LIBUSB__)
#include <libusb.h>
#endif

#include "Common/Assert.h"
#include "Common/Flag.h"
#include "Common/Thread.h"
#include "Core/LibusbUtils.h"

namespace LibusbUtils
{
#if defined(__LIBUSB__)
class Context::Impl
{
public:
  Impl()
  {
    const int ret = libusb_init(&m_context);
    ASSERT_MSG(IOS_USB, ret == LIBUSB_SUCCESS, "Failed to init libusb: %s", libusb_error_name(ret));
    if (ret != LIBUSB_SUCCESS)
      return;

#ifdef _WIN32
    libusb_set_option(m_context, LIBUSB_OPTION_USE_USBDK);
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

  libusb_context* GetContext() { return m_context; }

  bool GetDeviceList(GetDeviceListCallback callback)
  {
    std::lock_guard lock{m_device_list_mutex};

    libusb_device** list;
    ssize_t count = libusb_get_device_list(m_context, &list);
    if (count < 0)
      return false;

    for (ssize_t i = 0; i < count; ++i)
    {
      if (!callback(list[i]))
        break;
    }
    libusb_free_device_list(list, 1);
    return true;
  }

private:
  void EventThread()
  {
    Common::SetCurrentThreadName("libusb thread");
    timeval tv{5, 0};
    while (m_event_thread_running.IsSet())
      libusb_handle_events_timeout_completed(m_context, &tv, nullptr);
  }

  libusb_context* m_context = nullptr;
  std::mutex m_device_list_mutex;
  Common::Flag m_event_thread_running;
  std::thread m_event_thread;
};
#else
class Context::Impl
{
public:
  libusb_context* GetContext() { return nullptr; }
  bool GetDeviceList(GetDeviceListCallback callback) { return false; }
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

bool Context::GetDeviceList(GetDeviceListCallback callback)
{
  return m_impl->GetDeviceList(std::move(callback));
}

ConfigDescriptor MakeConfigDescriptor(libusb_device* device, u8 config_num)
{
#if defined(__LIBUSB__)
  libusb_config_descriptor* descriptor = nullptr;
  if (libusb_get_config_descriptor(device, config_num, &descriptor) == LIBUSB_SUCCESS)
    return {descriptor, libusb_free_config_descriptor};
#endif
  return {nullptr, [](auto) {}};
}
}  // namespace LibusbUtils
