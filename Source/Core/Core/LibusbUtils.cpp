// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#if defined(__LIBUSB__)
#include <libusb.h>
#endif

#include "Common/Assert.h"
#include "Common/Event.h"
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

  int SubmitTransfer(libusb_transfer* transfer, TransferCallback callback)
  {
    {
      std::lock_guard lock{m_transfer_callbacks_mutex};
      m_transfer_callbacks.emplace(transfer, std::move(callback));
    }
    ASSERT(!transfer->user_data && !transfer->callback);
    transfer->user_data = this;
    transfer->callback = [](libusb_transfer* tr) {
      static_cast<Impl*>(tr->user_data)->HandleTransferCallback(tr);
    };
    return libusb_submit_transfer(transfer);
  }

  s64 SubmitTransferSync(libusb_transfer* transfer)
  {
    Common::Event transfer_done_event;
    std::atomic<s64> ret;

    const int submit_ret = SubmitTransfer(transfer, [&] {
      ret = (transfer->status == 0) ? transfer->actual_length : LIBUSB_ERROR_OTHER;
      transfer_done_event.Set();
    });

    if (submit_ret != 0)
      return submit_ret;

    transfer_done_event.Wait();
    return ret;
  }

private:
  void EventThread()
  {
    Common::SetCurrentThreadName("libusb thread");
    timeval tv{5, 0};
    while (m_event_thread_running.IsSet())
      libusb_handle_events_timeout_completed(m_context, &tv, nullptr);
  }

  TransferCallback PopTransferCallback(libusb_transfer* transfer)
  {
    std::lock_guard lock{m_transfer_callbacks_mutex};
    auto it = m_transfer_callbacks.find(transfer);
    TransferCallback callback = std::move(it->second);
    m_transfer_callbacks.erase(it);
    return callback;
  }

  void HandleTransferCallback(libusb_transfer* transfer)
  {
    const auto callback = PopTransferCallback(transfer);
    callback();
  }

  libusb_context* m_context = nullptr;
  std::mutex m_device_list_mutex;
  std::mutex m_transfer_callbacks_mutex;
  std::map<libusb_transfer*, TransferCallback> m_transfer_callbacks;
  Common::Flag m_event_thread_running;
  std::thread m_event_thread;
};
#else
class Context::Impl
{
public:
  libusb_context* GetContext() { return nullptr; }
  bool GetDeviceList(GetDeviceListCallback callback) { return false; }
  int SubmitTransfer(libusb_transfer*, TransferCallback) { return -1; }
  int SubmitTransferSync(libusb_transfer* transfer) { return -1; }
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

int Context::SubmitTransfer(libusb_transfer* transfer, TransferCallback callback)
{
  return m_impl->SubmitTransfer(transfer, std::move(callback));
}

s64 Context::SubmitTransferSync(libusb_transfer* transfer)
{
  return m_impl->SubmitTransferSync(transfer);
}

s64 Context::ControlTransfer(libusb_device_handle* handle, u8 bmRequestType, u8 bRequest,
                             u16 wValue, u16 wIndex, u8* data, u16 wLength, u32 timeout)
{
#if defined(__LIBUSB__)
  auto transfer = MakeTransfer();
  std::vector<u8> buffer(LIBUSB_CONTROL_SETUP_SIZE + wLength);

  libusb_fill_control_setup(buffer.data(), bmRequestType, bRequest, wValue, wIndex, wLength);
  if ((bmRequestType & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT)
    std::copy_n(data, wLength, buffer.data() + LIBUSB_CONTROL_SETUP_SIZE);
  libusb_fill_control_transfer(transfer.get(), handle, buffer.data(), nullptr, nullptr, timeout);

  const int ret = SubmitTransferSync(transfer.get());
  if (ret < 0)
    return ret;

  if ((bmRequestType & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)
    std::copy_n(buffer.data() + LIBUSB_CONTROL_SETUP_SIZE, transfer->actual_length, data);

  return ret;
#else
  return -1;
#endif
}

s64 Context::InterruptTransfer(libusb_device_handle* handle, u8 endpoint, u8* data, int length,
                               u32 timeout)
{
#if defined(__LIBUSB__)
  auto transfer = MakeTransfer();
  libusb_fill_interrupt_transfer(transfer.get(), handle, endpoint, data, length, nullptr, nullptr,
                                 timeout);
  return SubmitTransferSync(transfer.get());
#else
  return -1;
#endif
}

s64 Context::BulkTransfer(libusb_device_handle* handle, u8 endpoint, u8* data, int length,
                          u32 timeout)
{
#if defined(__LIBUSB__)
  auto transfer = MakeTransfer();
  libusb_fill_bulk_transfer(transfer.get(), handle, endpoint, data, length, nullptr, nullptr,
                            timeout);
  return SubmitTransferSync(transfer.get());
#else
  return -1;
#endif
}

Context& GetContext()
{
  static Context s_context;
  return s_context;
}

Transfer MakeTransfer(int num_isoc_packets)
{
#if defined(__LIBUSB__)
  return {libusb_alloc_transfer(num_isoc_packets), libusb_free_transfer};
#else
  return {nullptr, [](auto) {}};
#endif
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
