// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/USBScanner.h"

#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <utility>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Emulated/Infinity.h"
#include "Core/IOS/USB/Emulated/Skylanders/Skylander.h"
#include "Core/IOS/USB/Host.h"
#include "Core/IOS/USB/LibusbDevice.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"

namespace IOS::HLE
{
USBScanner::USBScanner(USBHost* host) : m_host(host)
{
}

USBScanner::~USBScanner()
{
  Stop();
}

void USBScanner::WaitForFirstScan()
{
  if (m_thread_running.IsSet())
  {
    m_first_scan_complete_event.Wait();
  }
}

void USBScanner::Start()
{
  if (Core::WantsDeterminism())
  {
    UpdateDevices();
    return;
  }
  if (m_thread_running.TestAndSet())
  {
    m_thread = std::thread([this] {
      Common::SetCurrentThreadName("USB Scan Thread");
      while (m_thread_running.IsSet())
      {
        if (UpdateDevices())
          m_first_scan_complete_event.Set();
        Common::SleepCurrentThread(50);
      }
    });
  }
}

void USBScanner::Stop()
{
  if (m_thread_running.TestAndClear())
    m_thread.join();

  // Clear all devices and dispatch removal hooks.
  DeviceChangeHooks hooks;
  DetectRemovedDevices(std::set<u64>(), hooks);
  m_host->DispatchHooks(hooks);
}

// This is called from the scan thread. Returns false if we failed to update the device list.
bool USBScanner::UpdateDevices(const bool always_add_hooks)
{
  DeviceChangeHooks hooks;
  std::set<u64> plugged_devices;
  // If we failed to get a new, up-to-date list of devices, we cannot detect device removals.
  if (!AddNewDevices(plugged_devices, hooks, always_add_hooks))
    return false;
  DetectRemovedDevices(plugged_devices, hooks);
  m_host->DispatchHooks(hooks);
  return true;
}

bool USBScanner::AddDevice(std::unique_ptr<USB::Device> device)
{
  std::lock_guard lk(m_devices_mutex);
  if (m_devices.contains(device->GetId()))
    return false;

  m_devices[device->GetId()] = std::move(device);
  return true;
}

bool USBScanner::AddNewDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks,
                               const bool always_add_hooks)
{
  AddEmulatedDevices(new_devices, hooks, always_add_hooks);
#ifdef __LIBUSB__
  if (!Core::WantsDeterminism())
  {
    auto whitelist = Config::GetUSBDeviceWhitelist();
    if (whitelist.empty())
      return true;

    if (m_context.IsValid())
    {
      const int ret = m_context.GetDeviceList([&](libusb_device* device) {
        libusb_device_descriptor descriptor;
        libusb_get_device_descriptor(device, &descriptor);
        if (!whitelist.contains({descriptor.idVendor, descriptor.idProduct}))
          return true;

        auto usb_device =
            std::make_unique<USB::LibusbDevice>(m_host->GetEmulationKernel(), device, descriptor);
        CheckAndAddDevice(std::move(usb_device), new_devices, hooks, always_add_hooks);
        return true;
      });
      if (ret != LIBUSB_SUCCESS)
        WARN_LOG_FMT(IOS_USB, "GetDeviceList failed: {}", LibusbUtils::ErrorWrap(ret));
    }
  }
#endif
  return true;
}

void USBScanner::DetectRemovedDevices(const std::set<u64>& plugged_devices,
                                      DeviceChangeHooks& hooks)
{
  std::lock_guard lk(m_devices_mutex);
  for (auto it = m_devices.begin(); it != m_devices.end();)
  {
    if (!plugged_devices.contains(it->second->GetId()))
    {
      hooks.emplace(it->second, ChangeEvent::Removed);
      it = m_devices.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

void USBScanner::AddEmulatedDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks,
                                    bool always_add_hooks)
{
  if (Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL) && !NetPlay::IsNetPlayRunning())
  {
    auto skylanderportal = std::make_unique<USB::SkylanderUSB>(m_host->GetEmulationKernel());
    CheckAndAddDevice(std::move(skylanderportal), new_devices, hooks, always_add_hooks);
  }
  if (Config::Get(Config::MAIN_EMULATE_INFINITY_BASE) && !NetPlay::IsNetPlayRunning())
  {
    auto infinity_base = std::make_unique<USB::InfinityUSB>(m_host->GetEmulationKernel());
    CheckAndAddDevice(std::move(infinity_base), new_devices, hooks, always_add_hooks);
  }
}

void USBScanner::CheckAndAddDevice(std::unique_ptr<USB::Device> device, std::set<u64>& new_devices,
                                   DeviceChangeHooks& hooks, bool always_add_hooks)
{
  if (m_host->ShouldAddDevice(*device))
  {
    const u64 deviceid = device->GetId();
    new_devices.insert(deviceid);
    if (AddDevice(std::move(device)) || always_add_hooks)
    {
      hooks.emplace(GetDeviceById(deviceid), ChangeEvent::Inserted);
    }
  }
}

std::shared_ptr<USB::Device> USBScanner::GetDeviceById(const u64 device_id) const
{
  std::lock_guard lk(m_devices_mutex);
  const auto it = m_devices.find(device_id);
  if (it == m_devices.end())
    return nullptr;
  return it->second;
}

}  // namespace IOS::HLE
