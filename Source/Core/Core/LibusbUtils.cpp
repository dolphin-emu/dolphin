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
#include "Common/StringUtil.h"
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

std::optional<std::string> GetStringDescriptor(libusb_device_handle* dev_handle, uint8_t desc_index)
{
#if defined(__LIBUSB__)
  struct StringDescriptor
  {
    u8 length;
    u8 descriptor_type;
    char16_t data[127];
  };

  StringDescriptor buffer{};

  // Reading lang ID 0 returns a list of lang IDs.
  const auto lang_id_result = libusb_get_string_descriptor(
      dev_handle, desc_index, 0, reinterpret_cast<unsigned char*>(&buffer), 4);

  if (lang_id_result != 4 || buffer.length < 4 || buffer.descriptor_type != LIBUSB_DT_STRING)
  {
    ERROR_LOG_FMT(IOS_USB, "libusb_get_string_descriptor(desc_index={}, lang_id=0) result:{}",
                  int(desc_index), lang_id_result);
    return std::nullopt;
  }

  const u16 lang_id = buffer.data[0];

  const auto str_result = libusb_get_string_descriptor(
      dev_handle, desc_index, lang_id, reinterpret_cast<unsigned char*>(&buffer), 255);

  if (str_result < 2 || buffer.length > str_result || buffer.descriptor_type != LIBUSB_DT_STRING)
  {
    ERROR_LOG_FMT(IOS_USB, "libusb_get_string_descriptor(desc_index={}, lang_id={}) result:{}",
                  int(desc_index), lang_id, str_result);
    return std::nullopt;
  }

  // The UTF-16 data size should be even and equal to the return value.
  if ((buffer.length & 1u) != 0 || buffer.length != str_result)
  {
    WARN_LOG_FMT(IOS_USB, "GetStringDescriptor: buffer.length:{} result:{}", buffer.length,
                 str_result);
  }

  const std::size_t str_length = std::max(0, buffer.length - 2) / 2;
  return UTF16ToUTF8(std::u16string_view{buffer.data, str_length});
#else
  return "__LIBUSB__ not defined";
#endif
}

}  // namespace LibusbUtils
