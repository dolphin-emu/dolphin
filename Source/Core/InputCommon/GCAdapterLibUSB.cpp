// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/GCAdapterBackend.h"

#include <expected>
#include <memory>
#include <shared_mutex>
#include <stop_token>
#include <thread>

#include <libusb.h>

#include "Common/Logging/Log.h"
#include "Common/SPSCQueue.h"
#include "Common/ScopeGuard.h"
#include "Common/Thread.h"
#include "Core/LibusbUtils.h"

namespace GCAdapter
{
namespace
{
constexpr u16 VENDOR_ID = 0x057e;
constexpr u16 PRODUCT_ID = 0x0337;
constexpr int INTERFACE_NUMBER = 0;

// FYI: Users had issues in the past with 16ms when using USB over IP.
constexpr unsigned int TRANSFER_TIMEOUT_MS = 100;

struct USBDevice final
{
  libusb_device_handle* handle = nullptr;
  u8 endpoint_in = 0;
  u8 endpoint_out = 0;

  USBDevice() = default;

  USBDevice(const USBDevice&) = delete;
  USBDevice(USBDevice&&) = delete;
  USBDevice& operator=(const USBDevice&) = delete;
  USBDevice& operator=(USBDevice&&) = delete;

  ~USBDevice()
  {
    if (!handle)
      return;

    libusb_release_interface(handle, INTERFACE_NUMBER);
    libusb_close(handle);
  }

  int Read(std::span<u8> buffer)
  {
    int transferred = 0;
    const int result = libusb_interrupt_transfer(handle, endpoint_in, buffer.data(),
                                                 static_cast<int>(buffer.size()), &transferred,
                                                 TRANSFER_TIMEOUT_MS);

    return result == LIBUSB_SUCCESS ? transferred : result;
  }

  int Write(std::span<const u8> buffer)
  {
    int transferred = 0;
    const int result = libusb_interrupt_transfer(
        handle, endpoint_out, const_cast<unsigned char*>(buffer.data()),
        static_cast<int>(buffer.size()), &transferred, TRANSFER_TIMEOUT_MS);

    return result == LIBUSB_SUCCESS ? transferred : result;
  }
};

// Returns a libUSB error code when relevant.
std::expected<std::unique_ptr<USBDevice>, int> OpenDevice(libusb_device* device)
{
  libusb_device_descriptor desc{};
  int result = libusb_get_device_descriptor(device, &desc);

  if (result != LIBUSB_SUCCESS || desc.idVendor != VENDOR_ID || desc.idProduct != PRODUCT_ID)
  {
    return nullptr;
  }

  INFO_LOG_FMT(CONTROLLERINTERFACE,
               "Found GCAdapter [Bus:{:03d}, Address:{:03d}, VID:{:04X}, PID:{:04X}]",
               libusb_get_bus_number(device), libusb_get_device_address(device), desc.idVendor,
               desc.idProduct);

  auto usb_device = std::make_unique<USBDevice>();

  result = libusb_open(device, &usb_device->handle);
  if (result != LIBUSB_SUCCESS)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_open failed: {}", LibusbUtils::ErrorWrap{result});
    return std::unexpected{result};
  }

  // On macos detaching would fail without root or entitlement.
  // We assume user is using GCAdapterDriver and therefore don't want to detach anything
  // TODO: Should FreeBSD et al. skip `libusb_detach_kernel_driver` as well ?
  // Or can/should we just check for LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER ?
  // Or maybe the `LIBUSB_ERROR_NOT_SUPPORTED` check here is enough already ?

  result = libusb_kernel_driver_active(usb_device->handle, INTERFACE_NUMBER);
  if (result == 1)
  {
#if !defined(__APPLE__)
    result = libusb_detach_kernel_driver(usb_device->handle, INTERFACE_NUMBER);
    if (result != LIBUSB_SUCCESS)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_detach_kernel_driver failed: {}",
                    LibusbUtils::ErrorWrap{result});
      return std::unexpected{result};
    }
#endif
  }
  else if (result != 0 && result != LIBUSB_ERROR_NOT_SUPPORTED)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_kernel_driver_active failed: {}",
                  LibusbUtils::ErrorWrap{result});
  }

  // TODO: Consider using LIBUSB_OPTION_WINDOWS_EXCLUSIVE_CLAIM on Windows.
  // TODO: Consider using `libusb_set_auto_detach_kernel_driver`.

  result = libusb_claim_interface(usb_device->handle, INTERFACE_NUMBER);
  if (result != LIBUSB_SUCCESS)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "libusb_claim_interface failed: {}",
                  LibusbUtils::ErrorWrap{result});
    return std::unexpected{result};
  }

  libusb_config_descriptor* config = nullptr;
  result = libusb_get_active_config_descriptor(device, &config);
  if (result != LIBUSB_SUCCESS)
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE, "libusb_get_active_config_descriptor failed: {}",
                 LibusbUtils::ErrorWrap{result});
    return std::unexpected{result};
  }

  Common::ScopeGuard free_config{[&] { libusb_free_config_descriptor(config); }};

  for (const auto& iface_container : std::span{config->interface, config->bNumInterfaces})
  {
    for (const auto& iface : std::span(iface_container.altsetting, iface_container.num_altsetting))
    {
      if (iface.bInterfaceNumber != INTERFACE_NUMBER)
        continue;

      for (const auto& endpoint : std::span{iface.endpoint, iface.bNumEndpoints})
      {
        if (endpoint.bEndpointAddress & LIBUSB_ENDPOINT_IN)
          usb_device->endpoint_in = endpoint.bEndpointAddress;
        else
          usb_device->endpoint_out = endpoint.bEndpointAddress;
      }
    }
  }

  if (!usb_device->endpoint_in || !usb_device->endpoint_out)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter endpoints were not found");
    return std::unexpected{LIBUSB_ERROR_NOT_FOUND};
  }

  // This call makes Nyko-brand (and perhaps other) adapters work.
  // However it returns LIBUSB_ERROR_PIPE with Mayflash adapters.
  const int control_result = libusb_control_transfer(usb_device->handle, 0x21, 11, 0x0001, 0,
                                                     nullptr, 0, TRANSFER_TIMEOUT_MS);
  if (control_result < LIBUSB_SUCCESS)
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE, "libusb_control_transfer failed: {}",
                 LibusbUtils::ErrorWrap{control_result});
  }

  const std::array<u8, 1> init_payload = {0x13};
  if (!usb_device->Write(init_payload))
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "GCAdapter initialization transfer failed");
  }

  return usb_device;
}

class LibUSBBackend final : public Backend
{
public:
  LibUSBBackend(Callbacks callbacks) : Backend{std::move(callbacks)}
  {
    m_hotplug_thread = std::jthread{[this](std::stop_token stoken) {
      if (RegisterHotplugCallback())
        HotplugThreadFunc(stoken);
      else
        ScanningThreadFunc(stoken);
    }};
  }

  bool IsConnected(std::string* error_message = nullptr) const override
  {
    const auto err = m_libusb_error.load(std::memory_order_relaxed);

    if (error_message != nullptr)
    {
      if (err != LIBUSB_SUCCESS && err != LIBUSB_ERROR_NO_DEVICE)
        *error_message = libusb_strerror(err);
    }

    return err == LIBUSB_SUCCESS;
  }

  int Read(std::span<u8> buffer) override
  {
    std::shared_lock lk{m_device_mutex};
    if (!m_device)
      return -1;

    return m_device->Read(buffer);
  }

  int Write(std::span<const u8> buffer) override
  {
    std::shared_lock lk{m_device_mutex};
    if (!m_device)
      return -1;

    return m_device->Write(buffer);
  }

private:
  void UpdateLibUSBError(int err)
  {
    m_libusb_error.store((err < 0) ? err : LIBUSB_SUCCESS, std::memory_order_relaxed);
  }

  void TryToAddFromEnumeratedDevices()
  {
    const int error =
        m_context.GetDeviceList([this](libusb_device* dev) { return !TryToUseDevice(dev); });
    if (error != LIBUSB_SUCCESS)
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "GetDeviceList failed: {}", LibusbUtils::ErrorWrap{error});
  }

  bool TryToUseDevice(libusb_device* dev)
  {
    std::expected result = OpenDevice(dev);
    if (result == nullptr)
      return false;

    if (!result.has_value())
    {
      UpdateLibUSBError(result.error());
      return false;
    }

    {
      std::lock_guard lk{m_device_mutex};
      m_device = std::move(result.value());
    }

    UpdateLibUSBError(LIBUSB_SUCCESS);
    InvokeCallback(&Callbacks::on_connected);
    return true;
  }

  void DisconnectDevice()
  {
    {
      std::lock_guard lk{m_device_mutex};
      m_device.reset();
    }

    UpdateLibUSBError(LIBUSB_ERROR_NO_DEVICE);
    InvokeCallback(&Callbacks::on_disconnected);
  }

  bool RegisterHotplugCallback()
  {
    if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) == 0)
      return false;

    auto hotplug_callback = [this](libusb_device* dev, libusb_hotplug_event event) {
      // FYI: libUSB forbids "blocking" functions in a hotplug callback.
      // This includes our synchronous IO and even `libusb_release_interface`.
      // We effectively can't do anything here so we queue the event for our thread to handle.
      m_hotplug_events.Emplace(dev, event);
    };

    m_hotplug_handle = LibusbUtils::CreateScopedHotplugCallback(
        m_context,
        libusb_hotplug_event(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                             LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
        LIBUSB_HOTPLUG_ENUMERATE, VENDOR_ID, PRODUCT_ID, LIBUSB_HOTPLUG_MATCH_ANY,
        std::move(hotplug_callback));

    return m_hotplug_handle != nullptr;
  }

  void HotplugThreadFunc(std::stop_token stoken)
  {
    Common::SetCurrentThreadName("GCAdapter Hotplug");

    // A shutdown kick.
    std::stop_callback cb{stoken, [this] { m_hotplug_events.Emplace(); }};

    while (true)
    {
      m_hotplug_events.WaitForData();

      if (stoken.stop_requested())
        break;

      const HotplugEvent event = m_hotplug_events.Front();
      m_hotplug_events.Pop();

      if (event.event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
      {
        if (!m_device)
          TryToUseDevice(event.dev);
      }
      else if (event.event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
      {
        if (!m_device || libusb_get_device(m_device->handle) != event.dev)
        {
          // We aren't using this device.
          continue;
        }

        DisconnectDevice();

        // Wait a bit after disconnect.
        if (Common::InterruptableSleepFor(stoken, std::chrono::milliseconds{500}))
          break;

        // Try without a hotplug event in case a second adapter exists.
        TryToAddFromEnumeratedDevices();
      }
    }
  }

  // This is used when hotplug functionality is unavailable.
  void ScanningThreadFunc(std::stop_token stoken)
  {
    Common::SetCurrentThreadName("GCAdapter Scanning");

    while (!stoken.stop_requested())
    {
      if (!m_device)
      {
        TryToAddFromEnumeratedDevices();
      }
      else
      {
        int config{};
        if (libusb_get_configuration(m_device->handle, &config) == LIBUSB_ERROR_NO_DEVICE)
        {
          DisconnectDevice();
        }
      }

      Common::InterruptableSleepFor(stoken, std::chrono::milliseconds{1000});
    }
  }

  LibusbUtils::Context m_context;

  // Used for connection status and error display in the UI.
  std::atomic<int> m_libusb_error{LIBUSB_ERROR_NO_DEVICE};

  std::shared_mutex m_device_mutex;
  // Note: Only Hotplug/Scanning thread changes this, so it can read it without holding the mutex.
  std::unique_ptr<USBDevice> m_device;

  struct HotplugEvent
  {
    libusb_device* dev{};
    libusb_hotplug_event event;
  };
  Common::WaitableSPSCQueue<HotplugEvent> m_hotplug_events;

  std::jthread m_hotplug_thread;

  std::shared_ptr<void> m_hotplug_handle;
};
}  // namespace

std::unique_ptr<Backend> CreateBackend(Backend::Callbacks callbacks)
{
  return std::make_unique<LibUSBBackend>(std::move(callbacks));
}
}  // namespace GCAdapter
