// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/USBScanner.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <ranges>
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
USBScanner::~USBScanner()
{
  StopScanning();
}

void USBScanner::WaitForFirstScan()
{
  if (m_thread_running.IsSet())
  {
    m_first_scan_complete_flag.Wait(true);
  }
}

bool USBScanner::AddClient(USBHost* client)
{
  bool should_start_scanning = false;
  bool client_is_new = true;

  std::lock_guard thread_lock(m_thread_start_stop_mutex);

  {
    std::lock_guard clients_lock(m_clients_mutex);
    should_start_scanning = m_clients.empty();
    auto [it, newly_added] = m_clients.insert(client);
    client_is_new = newly_added;
  }

  if (should_start_scanning)
    StartScanning();

  return client_is_new;
}

bool USBScanner::RemoveClient(USBHost* client)
{
  std::lock_guard thread_lock(m_thread_start_stop_mutex);

  bool client_was_removed;
  bool should_join_scan_thread = false;

  {
    std::lock_guard clients_lock(m_clients_mutex);

    const bool was_empty = m_clients.empty();
    client_was_removed = m_clients.erase(client) != 0;

    const bool should_stop_scanning = !was_empty && m_clients.empty();
    if (should_stop_scanning)
      should_join_scan_thread = m_thread_running.TestAndClear();
  }

  // The scan thread might be trying to lock m_clients_mutex, so we need to not hold a lock on
  // m_clients_mutex when trying to join with the scan thread.
  if (should_join_scan_thread)
    m_thread.join();

  return client_was_removed;
}

void USBScanner::StartScanning()
{
  if (m_thread_running.TestAndSet())
  {
    m_thread = std::thread([this] {
      Common::SetCurrentThreadName("USB Scan Thread");
      while (m_thread_running.IsSet())
      {
        if (UpdateDevices())
          m_first_scan_complete_flag.Set(true);
        Common::SleepCurrentThread(50);
      }
      m_devices.clear();
      m_first_scan_complete_flag.Set(false);
    });
  }
}

void USBScanner::StopScanning()
{
  if (m_thread_running.TestAndClear())
    m_thread.join();
}

USBScanner::DeviceMap USBScanner::GetDevices() const
{
  std::lock_guard lk(m_devices_mutex);
  return m_devices;
}

// This is called from the scan thread. Returns false if we failed to update the device list.
bool USBScanner::UpdateDevices()
{
  DeviceMap new_devices;
  if (!AddNewDevices(&new_devices))
    return false;

  std::lock_guard lk(m_devices_mutex);
  if (!std::ranges::equal(std::views::keys(m_devices), std::views::keys(new_devices)))
  {
    m_devices = std::move(new_devices);

    std::lock_guard clients_lock(m_clients_mutex);
    for (USBHost* client : m_clients)
      client->OnDevicesChanged(m_devices);
  }

  return true;
}

bool USBScanner::AddNewDevices(DeviceMap* new_devices) const
{
  AddEmulatedDevices(new_devices);
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

        auto usb_device = std::make_unique<USB::LibusbDevice>(device, descriptor);
        AddDevice(std::move(usb_device), new_devices);
        return true;
      });
      if (ret != LIBUSB_SUCCESS)
        WARN_LOG_FMT(IOS_USB, "GetDeviceList failed: {}", LibusbUtils::ErrorWrap(ret));
    }
  }
#endif
  return true;
}

void USBScanner::AddEmulatedDevices(DeviceMap* new_devices)
{
  if (Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL) && !NetPlay::IsNetPlayRunning())
  {
    auto skylanderportal = std::make_unique<USB::SkylanderUSB>();
    AddDevice(std::move(skylanderportal), new_devices);
  }
  if (Config::Get(Config::MAIN_EMULATE_INFINITY_BASE) && !NetPlay::IsNetPlayRunning())
  {
    auto infinity_base = std::make_unique<USB::InfinityUSB>();
    AddDevice(std::move(infinity_base), new_devices);
  }
}

void USBScanner::AddDevice(std::unique_ptr<USB::Device> device, DeviceMap* new_devices)
{
  (*new_devices)[device->GetId()] = std::move(device);
}

}  // namespace IOS::HLE
