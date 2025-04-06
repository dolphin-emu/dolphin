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
    m_host->OnDevicesChanged(m_devices);
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

        auto usb_device =
            std::make_unique<USB::LibusbDevice>(m_host->GetEmulationKernel(), device, descriptor);
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

void USBScanner::AddEmulatedDevices(DeviceMap* new_devices) const
{
  if (Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL) && !NetPlay::IsNetPlayRunning())
  {
    auto skylanderportal = std::make_unique<USB::SkylanderUSB>(m_host->GetEmulationKernel());
    AddDevice(std::move(skylanderportal), new_devices);
  }
  if (Config::Get(Config::MAIN_EMULATE_INFINITY_BASE) && !NetPlay::IsNetPlayRunning())
  {
    auto infinity_base = std::make_unique<USB::InfinityUSB>(m_host->GetEmulationKernel());
    AddDevice(std::move(infinity_base), new_devices);
  }
}

void USBScanner::AddDevice(std::unique_ptr<USB::Device> device, DeviceMap* new_devices) const
{
  if (m_host->ShouldAddDevice(*device))
    (*new_devices)[device->GetId()] = std::move(device);
}

}  // namespace IOS::HLE
