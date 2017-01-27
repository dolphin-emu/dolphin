// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/LibusbContext.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Host.h"
#include "Core/IOS/USB/LibusbDevice.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
USBHost::USBHost(u32 device_id, const std::string& device_name) : Device(device_id, device_name)
{
#ifdef __LIBUSB__
  m_libusb_context = LibusbContext::Get();
  _assert_msg_(IOS_USB, m_libusb_context, "Failed to init libusb.");
#endif
}

ReturnCode USBHost::Open(const OpenRequest& request)
{
  // Force a device scan to complete, because some games (including Your Shape) only care
  // about the initial device list (in the first GETDEVICECHANGE reply).
  while (!UpdateDevices())
  {
  }
  StartThreads();
  return IPC_SUCCESS;
}

void USBHost::UpdateWantDeterminism(const bool new_want_determinism)
{
  if (new_want_determinism)
    StopThreads();
  else if (IsOpened())
    StartThreads();
}

void USBHost::DoState(PointerWrap& p)
{
  if (IsOpened() && p.GetMode() == PointerWrap::MODE_READ)
  {
    // After a state has loaded, there may be insertion hooks for devices that were
    // already plugged in, and which need to be triggered.
    UpdateDevices(true);
  }
}

bool USBHost::AddDevice(std::unique_ptr<USB::Device> device)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  if (m_devices.count(device->GetId()) != 0)
    return false;

  m_devices[device->GetId()] = std::move(device);
  return true;
}

std::shared_ptr<USB::Device> USBHost::GetDeviceById(const u64 device_id) const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  const auto it = m_devices.find(device_id);
  if (it == m_devices.end())
    return nullptr;
  return it->second;
}

// This is called from the scan thread. Returns false if we failed to update the device list.
bool USBHost::UpdateDevices(const bool always_add_hooks)
{
  if (Core::g_want_determinism)
    return true;

  DeviceChangeHooks hooks;
  std::set<u64> plugged_devices;
  // If we failed to get a new, up-to-date list of devices, we cannot detect device removals.
  if (!AddNewDevices(&plugged_devices, &hooks, always_add_hooks))
    return false;
  DetectRemovedDevices(plugged_devices, &hooks);
  DispatchHooks(hooks);
  return true;
}

bool USBHost::AddNewDevices(std::set<u64>* plugged_devices, DeviceChangeHooks* hooks,
                            const bool always_add_hooks)
{
#ifdef __LIBUSB__
  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(m_libusb_context.get(), &list);
  if (cnt < 0)
  {
    WARN_LOG(IOS_USB, "Failed to get device list: %s", libusb_error_name(static_cast<int>(cnt)));
    return false;
  }

  // Add new devices
  for (ssize_t i = 0; i < cnt; ++i)
  {
    libusb_device* device = list[i];
    libusb_device_descriptor descr;
    libusb_get_device_descriptor(device, &descr);
    if (!SConfig::GetInstance().IsUSBDeviceWhitelisted({descr.idVendor, descr.idProduct}))
      continue;

    auto dev = std::make_unique<USB::LibusbDevice>(device);
    if (!ShouldAddDevice(*dev))
      continue;
    const u64 id = dev->GetId();
    plugged_devices->insert(id);
    if (AddDevice(std::move(dev)) || always_add_hooks)
      hooks->emplace(GetDeviceById(id), ChangeEvent::Inserted);
  }
  libusb_free_device_list(list, 1);
#endif
  return true;
}

void USBHost::DetectRemovedDevices(const std::set<u64>& plugged_devices, DeviceChangeHooks* hooks)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  for (auto it = m_devices.begin(); it != m_devices.end();)
  {
    if (!plugged_devices.count(it->second->GetId()))
    {
      hooks->emplace(it->second, ChangeEvent::Removed);
      it = m_devices.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

void USBHost::DispatchHooks(const DeviceChangeHooks& hooks)
{
  for (const auto& hook : hooks)
  {
    INFO_LOG(IOS_USB, "%s - %s device: %04x:%04x", GetDeviceName().c_str(),
             hook.second == ChangeEvent::Inserted ? "New" : "Removed", hook.first->GetVid(),
             hook.first->GetPid());
    OnDeviceChange(hook.second, hook.first);
  }
  if (!hooks.empty())
    OnDeviceChangeEnd();
}

void USBHost::StartThreads()
{
  if (Core::g_want_determinism)
    return;

  if (!m_scan_thread_running.IsSet())
  {
    m_scan_thread_running.Set();
    m_scan_thread = std::thread([this] {
      Common::SetCurrentThreadName("USB Scan Thread");
      while (m_scan_thread_running.IsSet())
      {
        UpdateDevices();
        Common::SleepCurrentThread(50);
      }
    });
  }

#ifdef __LIBUSB__
  if (!m_thread_running.IsSet())
  {
    m_thread_running.Set();
    m_thread = std::thread([this] {
      Common::SetCurrentThreadName("USB Passthrough Thread");
      while (m_thread_running.IsSet())
      {
        static timeval tv = {0, 50000};
        libusb_handle_events_timeout_completed(m_libusb_context.get(), &tv, nullptr);
      }
    });
  }
#endif
}

void USBHost::StopThreads()
{
  if (m_scan_thread_running.TestAndClear())
    m_scan_thread.join();

  // Clear all devices and dispatch removal hooks.
  DeviceChangeHooks hooks;
  DetectRemovedDevices(std::set<u64>(), &hooks);
  DispatchHooks(hooks);
  if (m_thread_running.TestAndClear())
    m_thread.join();
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
