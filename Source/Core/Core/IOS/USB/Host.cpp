// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Host.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Emulated/Infinity.h"
#include "Core/IOS/USB/Emulated/Skylanders/Skylander.h"
#include "Core/IOS/USB/Emulated/WiiSpeak.h"
#include "Core/IOS/USB/LibusbDevice.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"

namespace IOS::HLE
{
USBHost::USBHost(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

USBHost::~USBHost() = default;

std::optional<IPCReply> USBHost::Open(const OpenRequest& request)
{
  if (!m_has_initialised)
  {
    GetScanThread().Start();
    // Force a device scan to complete, because some games (including Your Shape) only care
    // about the initial device list (in the first GETDEVICECHANGE reply).
    GetScanThread().WaitForFirstScan();
    m_has_initialised = true;
  }
  return IPCReply(IPC_SUCCESS);
}

void USBHost::UpdateWantDeterminism(const bool new_want_determinism)
{
  if (new_want_determinism)
    GetScanThread().Stop();
  else if (IsOpened())
    GetScanThread().Start();
}

void USBHost::DoState(PointerWrap& p)
{
  Device::DoState(p);
  if (IsOpened() && p.IsReadMode())
  {
    // After a state has loaded, there may be insertion hooks for devices that were
    // already plugged in, and which need to be triggered.
    UpdateDevices(true);
  }
}

bool USBHost::AddDevice(std::unique_ptr<USB::Device> device)
{
  std::lock_guard lk(m_devices_mutex);
  if (m_devices.contains(device->GetId()))
    return false;

  m_devices[device->GetId()] = std::move(device);
  return true;
}

std::shared_ptr<USB::Device> USBHost::GetDeviceById(const u64 device_id) const
{
  std::lock_guard lk(m_devices_mutex);
  const auto it = m_devices.find(device_id);
  if (it == m_devices.end())
    return nullptr;
  return it->second;
}

void USBHost::OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> changed_device)
{
}

void USBHost::OnDeviceChangeEnd()
{
}

bool USBHost::ShouldAddDevice(const USB::Device& device) const
{
  return true;
}

void USBHost::Update()
{
  if (Core::WantsDeterminism())
    UpdateDevices();
}

// This is called from the scan thread. Returns false if we failed to update the device list.
bool USBHost::UpdateDevices(const bool always_add_hooks)
{
  DeviceChangeHooks hooks;
  std::set<u64> plugged_devices;
  // If we failed to get a new, up-to-date list of devices, we cannot detect device removals.
  if (!AddNewDevices(plugged_devices, hooks, always_add_hooks))
    return false;
  DetectRemovedDevices(plugged_devices, hooks);
  DispatchHooks(hooks);
  return true;
}

bool USBHost::AddNewDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks,
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
            std::make_unique<USB::LibusbDevice>(GetEmulationKernel(), device, descriptor);
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

void USBHost::DetectRemovedDevices(const std::set<u64>& plugged_devices, DeviceChangeHooks& hooks)
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

void USBHost::DispatchHooks(const DeviceChangeHooks& hooks)
{
  for (const auto& hook : hooks)
  {
    INFO_LOG_FMT(IOS_USB, "{} - {} device: {:04x}:{:04x}", GetDeviceName(),
                 hook.second == ChangeEvent::Inserted ? "New" : "Removed", hook.first->GetVid(),
                 hook.first->GetPid());
    OnDeviceChange(hook.second, hook.first);
  }
  if (!hooks.empty())
    OnDeviceChangeEnd();
}

void USBHost::AddEmulatedDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks,
                                 bool always_add_hooks)
{
  if (Config::Get(Config::MAIN_EMULATE_SKYLANDER_PORTAL) && !NetPlay::IsNetPlayRunning())
  {
    auto skylanderportal =
        std::make_unique<USB::SkylanderUSB>(GetEmulationKernel(), "Skylander Portal");
    CheckAndAddDevice(std::move(skylanderportal), new_devices, hooks, always_add_hooks);
  }
  if (Config::Get(Config::MAIN_EMULATE_INFINITY_BASE) && !NetPlay::IsNetPlayRunning())
  {
    auto infinity_base = std::make_unique<USB::InfinityUSB>(GetEmulationKernel(), "Infinity Base");
    CheckAndAddDevice(std::move(infinity_base), new_devices, hooks, always_add_hooks);
  }
  if (Config::Get(Config::MAIN_EMULATE_WII_SPEAK) && !NetPlay::IsNetPlayRunning())
  {
    auto wii_speak = std::make_unique<USB::WiiSpeak>(GetEmulationKernel());
    CheckAndAddDevice(std::move(wii_speak), new_devices, hooks, always_add_hooks);
  }
}

void USBHost::CheckAndAddDevice(std::unique_ptr<USB::Device> device, std::set<u64>& new_devices,
                                DeviceChangeHooks& hooks, bool always_add_hooks)
{
  if (ShouldAddDevice(*device))
  {
    const u64 deviceid = device->GetId();
    new_devices.insert(deviceid);
    if (AddDevice(std::move(device)) || always_add_hooks)
    {
      hooks.emplace(GetDeviceById(deviceid), ChangeEvent::Inserted);
    }
  }
}

USBHost::ScanThread::~ScanThread()
{
  Stop();
}

void USBHost::ScanThread::WaitForFirstScan()
{
  if (m_thread_running.IsSet())
  {
    m_first_scan_complete_event.Wait();
  }
}

void USBHost::ScanThread::Start()
{
  if (Core::WantsDeterminism())
  {
    m_host->UpdateDevices();
    return;
  }
  if (m_thread_running.TestAndSet())
  {
    m_thread = std::thread([this] {
      Common::SetCurrentThreadName("USB Scan Thread");
      while (m_thread_running.IsSet())
      {
        if (m_host->UpdateDevices())
          m_first_scan_complete_event.Set();
        Common::SleepCurrentThread(50);
      }
    });
  }
}

void USBHost::ScanThread::Stop()
{
  if (m_thread_running.TestAndClear())
    m_thread.join();

  // Clear all devices and dispatch removal hooks.
  DeviceChangeHooks hooks;
  m_host->DetectRemovedDevices(std::set<u64>(), hooks);
  m_host->DispatchHooks(hooks);
}

std::optional<IPCReply> USBHost::HandleTransfer(std::shared_ptr<USB::Device> device, u32 request,
                                                std::function<s32()> submit) const
{
  if (!device)
    return IPCReply(IPC_ENOENT);

  const s32 ret = submit();
  if (ret == IPC_SUCCESS)
    return std::nullopt;

  ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Failed to submit transfer (request {}): {}",
                device->GetVid(), device->GetPid(), request, device->GetErrorName(ret));
  return IPCReply(ret <= 0 ? ret : IPC_EINVAL);
}
}  // namespace IOS::HLE
